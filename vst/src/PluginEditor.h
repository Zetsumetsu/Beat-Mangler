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

class BeatManglerEditor : public juce::AudioProcessorEditor,
                           private juce::Timer {
public:
    explicit BeatManglerEditor(BeatManglerProcessor&);
    ~BeatManglerEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void loadFile();
    void updateSliceInfo();

    BeatManglerProcessor& proc;
    ManglerLookAndFeel laf;

    juce::Slider chaosKnob, densityKnob, pitchKnob, degradeKnob;
    juce::Label chaosLabel, densityLabel, pitchLabel, degradeLabel, rateLabel,
        gridLabel;
    juce::Label titleLabel, subLabel, fileLabel, sliceInfoLabel, sliceReadout,
        footerLabel, transportLabel;
    juce::TextButton loadButton{"LOAD"};
    juce::TextButton snapButtons[3];
    juce::TextButton rateButtons[4];
    juce::TextButton gridButtons[2];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAtt,
        densityAtt, pitchAtt, degradeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatManglerEditor)
};
