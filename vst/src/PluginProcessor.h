#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "beat_mangler_dsp.h"

// Beat Mangler VST3 — minimal prototype.
// Wraps the portable DSP core (same engine as the future Daisy firmware):
// load a WAV, it auto-slices, the chaos macro re-sequences it live.

class BeatManglerProcessor : public juce::AudioProcessor {
public:
    BeatManglerProcessor();
    ~BeatManglerProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Beat Mangler"; }
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
    std::atomic<float> manChaos{0.0f}, manDensity{0.5f}, manPitch{0.5f},
        manDegrade{0.5f}, manRate{1.0f};
    std::atomic<int> manGrid{16}, manSnap{1};
    void engageAutoBypass(); // snapshot current params into the manual set
    beatmangler::Params manualParams() const;
    beatmangler::Params effectiveParams() const; // manual if bypassed
    std::atomic<int> sliceCount{0};
    std::atomic<int> barsCount{0};
    std::atomic<int> lastSlice{-1};

    // Message thread only. Suspends processing while the file loads + slices.
    // The file's bytes are kept and embedded in the plugin state, so the
    // sample is restored when the project is reopened.
    bool loadAudioFile(const juce::File& file);
    bool resliceCurrentFile(); // re-slice loaded file (e.g. grid changed)
    juce::String loadedFileName() const { return loadedFileName_; }
    double getHostTempo() const;
    beatmangler::Params currentParams() const;

private:
    // Slice the stashed file bytes into the engine at the given tempo.
    bool performLoad(double tempo);
    beatmangler::Engine engine_;
    juce::String loadedFileName_ = "no file loaded";
    juce::File loadedFile_;
    juce::MemoryBlock loadedData_; // raw bytes of the loaded file (for state)
    std::atomic<bool> pendingReload_{false}; // state restored: load on 1st block
    double fallbackPpq_ = 0.0; // free-run position when host gives none
    bool lastGate_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatManglerProcessor)
};
