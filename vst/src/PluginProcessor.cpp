#include "PluginProcessor.h"
#include "PluginEditor.h"

BeatManglerProcessor::BeatManglerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",
                                                  juce::AudioChannelSet::stereo(),
                                                  true)),
      apvts(*this, nullptr, "BeatManglerParams", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout
BeatManglerProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "chaos", "Chaos", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "density", "Density", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pitchTrim", "Pitch Trim", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "degradeTrim", "Degrade Trim", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "rate", "Rate",
        juce::StringArray{"1/4", "1/2", "1", "2"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "grid", "Grid",
        juce::StringArray{"16ths", "Beats"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "pitchSnap", "Pitch Snap",
        juce::StringArray{"Glide", "Semitone", "Scale"}, 1));
    return {params.begin(), params.end()};
}

double BeatManglerProcessor::getHostTempo() const {
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            return pos->getBpm().orFallback(120.0);
    return 120.0;
}

void BeatManglerProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    engine_.setSampleRate(sampleRate);
    fallbackPpq_ = 0.0;
}

bool BeatManglerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool BeatManglerProcessor::loadAudioFile(const juce::File& file) {
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() < 44)
        return false;
    // Fresh params + host tempo so bar detection sees the current grid.
    beatmangler::Params p = currentParams();
    engine_.setParams(p);
    engine_.setTempo(getHostTempo());
    suspendProcessing(true);
    const bool ok = engine_.loadWavBytes((const uint8_t*)data.getData(), data.getSize());
    if (ok) {
        loadedFileName_ = file.getFileName();
        loadedFile_ = file;
        sliceCount.store(engine_.numSlices());
        barsCount.store(engine_.numBars());
    }
    suspendProcessing(false);
    return ok;
}

bool BeatManglerProcessor::resliceCurrentFile() {
    if (!loadedFile_.existsAsFile()) return false;
    return loadAudioFile(loadedFile_);
}

beatmangler::Params BeatManglerProcessor::currentParams() const {
    static constexpr float rateVals[4] = {0.25f, 0.5f, 1.0f, 2.0f};
    beatmangler::Params p;
    p.chaos = apvts.getRawParameterValue("chaos")->load();
    p.density = apvts.getRawParameterValue("density")->load();
    p.pitchTrim = apvts.getRawParameterValue("pitchTrim")->load();
    p.degradeTrim = apvts.getRawParameterValue("degradeTrim")->load();
    const int rateIdx = (int)apvts.getRawParameterValue("rate")->load();
    p.rate = rateVals[juce::jlimit(0, 3, rateIdx)];
    const int gridIdx = (int)apvts.getRawParameterValue("grid")->load();
    p.gridDiv = (gridIdx == 1) ? 4 : 16;
    p.pitchSnap = (beatmangler::PitchSnap)(int)apvts.getRawParameterValue("pitchSnap")->load();
    return p;
}

void BeatManglerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // MIDI input exists so hosts accept us as an instrument; incoming
    // MIDI is ignored — the sequencer locks to the host transport.
    (void)midiMessages;

    // Follow the host transport: steps lock to the song's PPQ grid, so
    // beat 1 lines up with beat 1, and we start/stop with the song.
    bool hostPlaying = true;
    double ppq = fallbackPpq_;
    bool havePosition = false;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            havePosition = true;
            hostPlaying = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback(0.0);
            engine_.setTempo(pos->getBpm().orFallback(120.0));
        }
    }
    if (!havePosition) {
        // No transport info (some hosts/tools): free-run so it still
        // makes sound, advancing our own PPQ at the current tempo.
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        fallbackPpq_ += (double)numSamples * (engine_.tempo() / 60.0) / sr;
    }
    transportPlaying.store(hostPlaying);

    if (engine_.numSlices() == 0)
        return;

    engine_.setParams(currentParams());
    engine_.processSynced(buffer.getWritePointer(0), buffer.getWritePointer(1),
                          numSamples, ppq, hostPlaying);
    lastSlice.store(engine_.lastSlice());
}

void BeatManglerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void BeatManglerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* BeatManglerProcessor::createEditor() {
    return new BeatManglerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BeatManglerProcessor();
}
