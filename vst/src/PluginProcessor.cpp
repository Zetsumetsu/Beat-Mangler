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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bpm", "BPM", 40.0f, 240.0f, 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "pitchSnap", "Pitch Snap",
        juce::StringArray{"Glide", "Semitone", "Scale"}, 1));
    return {params.begin(), params.end()};
}

void BeatManglerProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    engine_.setSampleRate(sampleRate);
}

bool BeatManglerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool BeatManglerProcessor::loadAudioFile(const juce::File& file) {
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() < 44)
        return false;
    suspendProcessing(true);
    const bool ok = engine_.loadWavBytes((const uint8_t*)data.getData(), data.getSize());
    if (ok) {
        loadedFileName_ = file.getFileName();
        sliceCount.store(engine_.numSlices());
    }
    suspendProcessing(false);
    return ok;
}

void BeatManglerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // MIDI input exists so hosts accept us as an instrument; v0.1 runs on
    // its internal transport and ignores incoming MIDI for now.
    (void)midiMessages;

    if (!playing.load() || engine_.numSlices() == 0)
        return;

    beatmangler::Params p;
    p.chaos = apvts.getRawParameterValue("chaos")->load();
    p.density = apvts.getRawParameterValue("density")->load();
    p.pitchTrim = apvts.getRawParameterValue("pitchTrim")->load();
    p.degradeTrim = apvts.getRawParameterValue("degradeTrim")->load();
    p.bpm = apvts.getRawParameterValue("bpm")->load();
    p.pitchSnap = (beatmangler::PitchSnap)(int)apvts.getRawParameterValue("pitchSnap")->load();
    engine_.setParams(p);

    // engine renders stereo; our bus layout is stereo-only
    engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
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
