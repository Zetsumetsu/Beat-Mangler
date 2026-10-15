#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

BreakManglerProcessor::BreakManglerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",
                                                  juce::AudioChannelSet::stereo(),
                                                  true)),
      apvts(*this, nullptr, "BreakManglerParams", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout
BreakManglerProcessor::createParameterLayout() {
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

double BreakManglerProcessor::getHostTempo() const {
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            return pos->getBpm().orFallback(120.0);
    return 120.0;
}

void BreakManglerProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    engine_.setSampleRate(sampleRate);
    fallbackPpq_ = 0.0;
}

bool BreakManglerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool BreakManglerProcessor::performLoad(double tempo) {
    if (loadedData_.getSize() < 44)
        return false;
    // Fresh params + tempo so bar detection sees the current grid.
    engine_.setParams(effectiveParams());
    engine_.setTempo(tempo);
    const bool ok = engine_.loadWavBytes(
        (const uint8_t*)loadedData_.getData(), loadedData_.getSize());
    if (ok) {
        sliceCount.store(engine_.numSlices());
        barsCount.store(engine_.numBars());
        publishWaveSnapshot();
    }
    return ok;
}

void BreakManglerProcessor::publishWaveSnapshot() {
    auto snap = std::make_shared<WaveSnapshot>();
    const auto& l = engine_.waveL();
    const auto& r = engine_.waveR();
    const size_t n = l.size();
    snap->totalSamples = (int)n;
    snap->bars = engine_.numBars();
    snap->mins.assign((size_t)WaveSnapshot::kColumns, 0.0f);
    snap->maxs.assign((size_t)WaveSnapshot::kColumns, 0.0f);
    if (n > 0) {
        for (int c = 0; c < WaveSnapshot::kColumns; ++c) {
            const size_t s0 = (size_t)c * n / (size_t)WaveSnapshot::kColumns;
            const size_t s1 = (size_t)(c + 1) * n / (size_t)WaveSnapshot::kColumns;
            float mn = 0.0f, mx = 0.0f;
            for (size_t i = s0; i < s1 && i < n; ++i) {
                const float lo = std::min(l[i], r[i]);
                const float hi = std::max(l[i], r[i]);
                if (lo < mn) mn = lo;
                if (hi > mx) mx = hi;
            }
            snap->mins[(size_t)c] = mn;
            snap->maxs[(size_t)c] = mx;
        }
        const int ns = engine_.numSlices();
        snap->sliceBounds.reserve((size_t)ns);
        for (int i = 0; i < ns; ++i) {
            const auto& s = engine_.slice(i);
            snap->sliceBounds.emplace_back(s.start, s.end);
        }
    }
    const juce::ScopedLock lock(waveSnapshotLock_);
    waveSnapshot_ = snap;
}

std::shared_ptr<const BreakManglerProcessor::WaveSnapshot>
BreakManglerProcessor::waveSnapshot() {
    const juce::ScopedLock lock(waveSnapshotLock_);
    return waveSnapshot_;
}

bool BreakManglerProcessor::loadAudioFile(const juce::File& file) {
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() < 44)
        return false;
    loadedData_ = data; // kept for project persistence
    suspendProcessing(true);
    const bool ok = performLoad(getHostTempo());
    if (ok) {
        loadedFileName_ = file.getFileName();
        loadedFile_ = file;
    }
    suspendProcessing(false);
    return ok;
}

bool BreakManglerProcessor::resliceCurrentFile() {
    if (loadedData_.getSize() < 44) return false;
    suspendProcessing(true);
    const bool ok = performLoad(getHostTempo());
    suspendProcessing(false);
    return ok;
}

breakmangler::Params BreakManglerProcessor::currentParams() const {
    static constexpr float rateVals[4] = {0.25f, 0.5f, 1.0f, 2.0f};
    breakmangler::Params p;
    p.chaos = apvts.getRawParameterValue("chaos")->load();
    p.density = apvts.getRawParameterValue("density")->load();
    p.pitchTrim = apvts.getRawParameterValue("pitchTrim")->load();
    p.degradeTrim = apvts.getRawParameterValue("degradeTrim")->load();
    const int rateIdx = (int)apvts.getRawParameterValue("rate")->load();
    p.rate = rateVals[juce::jlimit(0, 3, rateIdx)];
    const int gridIdx = (int)apvts.getRawParameterValue("grid")->load();
    p.gridDiv = (gridIdx == 1) ? 4 : 16;
    p.pitchSnap = (breakmangler::PitchSnap)(int)apvts.getRawParameterValue("pitchSnap")->load();
    return p;
}

void BreakManglerProcessor::engageAutoBypass() {
    const breakmangler::Params p = currentParams();
    manChaos.store(p.chaos);
    manDensity.store(p.density);
    manPitch.store(p.pitchTrim);
    manDegrade.store(p.degradeTrim);
    manRate.store(p.rate);
    manGrid.store(p.gridDiv);
    manSnap.store((int)p.pitchSnap);
    autoBypass.store(true);
}

breakmangler::Params BreakManglerProcessor::manualParams() const {
    breakmangler::Params p;
    p.chaos = manChaos.load();
    p.density = manDensity.load();
    p.pitchTrim = manPitch.load();
    p.degradeTrim = manDegrade.load();
    p.rate = manRate.load();
    p.gridDiv = manGrid.load();
    p.pitchSnap = (breakmangler::PitchSnap)manSnap.load();
    return p;
}

breakmangler::Params BreakManglerProcessor::effectiveParams() const {
    return autoBypass.load() ? manualParams() : currentParams();
}

void BreakManglerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
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
    double blockTempo = engine_.tempo();
    bool havePosition = false;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            havePosition = true;
            hostPlaying = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback(0.0);
            blockTempo = pos->getBpm().orFallback(120.0);
            engine_.setTempo(blockTempo);
        }
    }
    if (!havePosition) {
        // No transport info (some hosts/tools): free-run so it still
        // makes sound, advancing our own PPQ at the current tempo.
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        fallbackPpq_ += (double)numSamples * (engine_.tempo() / 60.0) / sr;
    }
    transportPlaying.store(hostPlaying);

    // Deferred project restore: slice the embedded sample now that the
    // real host tempo is known (bar detection depends on it).
    if (pendingReload_.load()) {
        pendingReload_.store(false);
        performLoad(blockTempo);
    }

    if (engine_.numSlices() == 0)
        return;

    // Panel gate drives the engine's launch control: closed stops now;
    // a rising edge arms a quantized restart at the next bar line.
    const bool g = gateOpen.load();
    if (!g)
        engine_.stopNow();
    else if (!lastGate_)
        engine_.armLaunch();
    lastGate_ = g;
    launchArmed.store(engine_.launchState() == breakmangler::Engine::Launch::Armed);

    engine_.setParams(effectiveParams());
    engine_.processSynced(buffer.getWritePointer(0), buffer.getWritePointer(1),
                          numSamples, ppq, hostPlaying);
    lastSlice.store(engine_.lastSlice());
    playheadStep.store(engine_.loopStep());
}

void BreakManglerProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return;
    xml->setAttribute("bmFile", loadedFileName_);
    xml->setAttribute("bmGate", gateOpen.load() ? 1 : 0);
    xml->setAttribute("bmAuto", autoBypass.load() ? 1 : 0);
    xml->setAttribute("bmMChaos", (double)manChaos.load());
    xml->setAttribute("bmMDensity", (double)manDensity.load());
    xml->setAttribute("bmMPitch", (double)manPitch.load());
    xml->setAttribute("bmMDegrade", (double)manDegrade.load());
    xml->setAttribute("bmMRate", (double)manRate.load());
    xml->setAttribute("bmMGrid", manGrid.load());
    xml->setAttribute("bmMSnap", manSnap.load());
    juce::MemoryBlock xmlBlock;
    copyXmlToBinary(*xml, xmlBlock);
    // Layout: "BMS1" | int64 xmlLen | xml | int64 audioLen | audio bytes.
    // The sample itself is embedded, so the project is self-contained.
    juce::MemoryOutputStream os(destData, false);
    os.write("BMS1", 4);
    os.writeInt64((juce::int64)xmlBlock.getSize());
    os.write(xmlBlock.getData(), xmlBlock.getSize());
    os.writeInt64((juce::int64)loadedData_.getSize());
    if (loadedData_.getSize() > 0)
        os.write(loadedData_.getData(), loadedData_.getSize());
}

void BreakManglerProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (sizeInBytes >= 4 && std::memcmp(data, "BMS1", 4) == 0) {
        juce::MemoryInputStream is(data, (size_t)sizeInBytes, false);
        is.setPosition(4);
        const auto xmlLen = is.readInt64();
        if (xmlLen > 0 && xmlLen <= sizeInBytes) {
            juce::MemoryBlock xmlBlock;
            xmlBlock.setSize((size_t)xmlLen);
            is.read(xmlBlock.getData(), (size_t)xmlLen);
            if (auto xml = getXmlFromBinary(xmlBlock.getData(),
                                            (int)xmlBlock.getSize())) {
                loadedFileName_ =
                    xml->getStringAttribute("bmFile", "no file loaded");
                gateOpen.store(xml->getIntAttribute("bmGate", 1) != 0);
                lastGate_ = gateOpen.load();
                if (!lastGate_) engine_.stopNow();
                manChaos.store((float)xml->getDoubleAttribute("bmMChaos", 0.0));
                manDensity.store((float)xml->getDoubleAttribute("bmMDensity", 0.5));
                manPitch.store((float)xml->getDoubleAttribute("bmMPitch", 0.5));
                manDegrade.store((float)xml->getDoubleAttribute("bmMDegrade", 0.5));
                manRate.store((float)xml->getDoubleAttribute("bmMRate", 1.0));
                manGrid.store(xml->getIntAttribute("bmMGrid", 16));
                manSnap.store(xml->getIntAttribute("bmMSnap", 1));
                autoBypass.store(xml->getIntAttribute("bmAuto", 0) != 0);
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
            }
        }
        const auto audioLen = is.readInt64();
        if (audioLen > 0 && audioLen <= sizeInBytes) {
            loadedData_.setSize((size_t)audioLen);
            is.read(loadedData_.getData(), (size_t)audioLen);
            // Sliced on the first audio block, once the real host tempo
            // is known (bar detection depends on it).
            pendingReload_.store(true);
        }
        return;
    }
    // Legacy (pre-v0.5) state: bare APVTS XML, no sample.
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* BreakManglerProcessor::createEditor() {
    return new BreakManglerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BreakManglerProcessor();
}
