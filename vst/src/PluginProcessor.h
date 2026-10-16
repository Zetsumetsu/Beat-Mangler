#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>
#include "break_mangler_dsp.h"

// Break Mangler VST3 — minimal prototype.
// Wraps the portable DSP core (same engine as the future Daisy firmware):
// load a WAV, it auto-slices, the chaos macro re-sequences it live.

class BreakManglerProcessor : public juce::AudioProcessor {
public:
    BreakManglerProcessor();
    ~BreakManglerProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Break Mangler"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // GUI-facing state (atomics; written on audio thread, read on message thread)
    std::atomic<bool> transportPlaying{false}; // mirrors the HOST transport
    std::atomic<bool> gateOpen{true}; // panel Play/Stop gate over the host sync
    std::atomic<bool> launchArmed{false}; // waiting for the next bar line

    // Automation bypass: when engaged, the engine runs on this manual
    // snapshot and host parameter automation is ignored until released.
    std::atomic<bool> autoBypass{false};
    std::atomic<float> manChaos{0.0f}, manDensity{0.0f}, manPitch{0.0f},
        manDegrade{0.0f}, manEra{0.0f}, manRate{1.0f};
    std::atomic<int> manGrid{16}, manSnap{1};
    void engageAutoBypass(); // snapshot current params into the manual set
    breakmangler::Params manualParams() const;
    breakmangler::Params effectiveParams() const; // manual if bypassed
    std::atomic<int> sliceCount{0};
    std::atomic<int> barsCount{0};
    std::atomic<int> lastSlice{-1};
    std::atomic<int> playheadStep{0}; // in-order step within the loop, for GUI

    // Waveform snapshot for the GUI: per-column peaks, slice bounds in
    // samples, bar count. Built after every load (message or audio thread)
    // and published under a lock; the editor renders from its own copy.
    struct WaveSnapshot {
        static constexpr int kColumns = 512;
        std::vector<float> mins, maxs; // per-column peak pairs, -1..1
        std::vector<std::pair<int,int>> sliceBounds; // sample ranges
        int bars = 0;
        int totalSamples = 0;
    };
    std::shared_ptr<const WaveSnapshot> waveSnapshot(); // locks briefly

    // Message thread only. Suspends processing while the file loads + slices.
    // The file's bytes are kept and embedded in the plugin state, so the
    // sample is restored when the project is reopened.
    bool loadAudioFile(const juce::File& file);
    bool resliceCurrentFile(); // re-slice loaded file (e.g. grid changed)

    // RESET: zero the five knobs (the clean sample). Works in both AUTO
    // modes: writes the host params in HOST mode, the manual snapshot in
    // MANUAL mode.
    void resetKnobsToZero();
    juce::String loadedFileName() const { return loadedFileName_; }
    breakmangler::Params currentParams() const;

private:
    // Slice the stashed file bytes into the engine at the given tempo.
    bool performLoad(double tempo);
    void publishWaveSnapshot(); // snapshot the fitted loop for the GUI
    breakmangler::Engine engine_;
    juce::String loadedFileName_ = "no file loaded";
    juce::File loadedFile_;
    juce::MemoryBlock loadedData_; // raw bytes of the loaded file (for state)
    std::atomic<bool> pendingReload_{false}; // state restored: load on 1st block
    juce::CriticalSection waveSnapshotLock_;
    std::shared_ptr<const WaveSnapshot> waveSnapshot_;
    double fallbackPpq_ = 0.0; // free-run position when host gives none
    bool lastGate_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreakManglerProcessor)
};
