#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// First-pass GUI: dark industrial panel, hero CHAOS knob, trim knobs,
// pitch-snap segmented control, file load, transport. My judgement call;
// iterate freely.

class ManglerLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    juce::Colour accentFor(juce::Component& c);
};

// Waveform screen: replaces the old gibberish slice readout. Shows the
// loaded loop on its slice grid; the currently sounding slice glows and a
// playhead line sweeps the bar grid. The waveform image is rendered once
// per snapshot; only the overlays repaint on the timer.
class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer {
public:
    WaveformDisplay();
    void setSnapshot(std::shared_ptr<const BreakManglerProcessor::WaveSnapshot> snap);
    void setPlayState(int step, int totalSteps, int activeSlice, bool rolling);
    void paint(juce::Graphics& g) override;
    void resized() override;
    // Drop zone: files dropped here load like the LOAD button.
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    std::function<void(const juce::File&)> onFileDrop;

private:
    void timerCallback() override;
    void rebuildImage();

    std::shared_ptr<const BreakManglerProcessor::WaveSnapshot> snap_;
    juce::Image waveImage_;
    int step_ = 0, totalSteps_ = 0, activeSlice_ = -1;
    bool rolling_ = false, dirty_ = true, dragOver_ = false;
};

class BreakManglerEditor : public juce::AudioProcessorEditor,
                           private juce::Timer {
public:
    explicit BreakManglerEditor(BreakManglerProcessor&);
    ~BreakManglerEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void loadFile();
    void updateSliceInfo();
    void setAutoBypass(bool on, bool snapshot);
    int shownSlices_ = -2;

public:
    // Display scramble: the title mangles itself while the engine plays.
    // intensity 0 = the true word, 1 = full gibberish (length preserved).
    static juce::String mangleWord(float intensity, juce::Random& rng);

private:
    juce::Random rng_;
    std::string manglerShown_ = "MANGLER"; // current (possibly scrambled) title word
    int lastSliceSeen_ = -1;

    BreakManglerProcessor& proc;
    ManglerLookAndFeel laf;
    WaveformDisplay waveform;

    juce::Slider chaosKnob, densityKnob, pitchKnob, degradeKnob;
    juce::Label chaosLabel, densityLabel, pitchLabel, degradeLabel, rateLabel,
        gridLabel, snapLabel;
    juce::Label titleLabel, subLabel, fileLabel, sliceInfoLabel,
        transportLabel;
    juce::TextButton loadButton{"LOAD"}, playButton{"STOP"}, autoButton{"AUTO: HOST"};
    juce::TextButton snapButtons[3];
    juce::TextButton rateButtons[4];
    juce::TextButton gridButtons[2];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAtt,
        densityAtt, pitchAtt, degradeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BreakManglerEditor)
};
