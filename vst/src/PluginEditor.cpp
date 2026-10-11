#include "PluginEditor.h"

namespace {
const juce::Colour kBg(0xff0e0e11);
const juce::Colour kPanel(0xff16161b);
const juce::Colour kRed(0xffff3b30);
const juce::Colour kSteel(0xff9aa0a8);
const juce::Colour kText(0xffe8e8e8);
const juce::Colour kDim(0xff7a7a84);
const juce::Colour kTrack(0xff2a2a30);
} // namespace

juce::Colour ManglerLookAndFeel::accentFor(juce::Component& c) {
    return c.getName() == "chaos" ? kRed : kSteel;
}

void ManglerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w,
                                           int h, float sliderPos,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider& slider) {
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float radius = std::min(w, h) * 0.5f - 6.0f;
    juce::Path track, value;
    track.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle,
                        rotaryEndAngle, true);
    const float angle =
        rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    value.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle,
                        true);
    g.setColour(kTrack);
    g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour(accentFor(slider));
    g.strokePath(value, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    // knob body
    g.setColour(kPanel);
    g.fillEllipse(cx - radius + 8, cy - radius + 8, (radius - 8) * 2,
                  (radius - 8) * 2);
    g.setColour(juce::Colour(0xff3a3a42));
    g.drawEllipse(cx - radius + 8, cy - radius + 8, (radius - 8) * 2,
                  (radius - 8) * 2, 1.0f);
    // pointer
    const float pr = radius - 12.0f;
    g.setColour(accentFor(slider));
    g.drawLine(cx + std::cos(angle - juce::MathConstants<float>::halfPi) * 4.0f,
               cy + std::sin(angle - juce::MathConstants<float>::halfPi) * 4.0f,
               cx + std::cos(angle - juce::MathConstants<float>::halfPi) * pr,
               cy + std::sin(angle - juce::MathConstants<float>::halfPi) * pr,
               3.0f);
}

void ManglerLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    juce::Colour fill = on ? kRed : kPanel;
    if (shouldDrawButtonAsDown) fill = fill.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted && !on) fill = juce::Colour(0xff202027);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(on ? kRed : juce::Colour(0xff3a3a42));
    g.drawRoundedRectangle(bounds, 5.0f, 1.5f);
    if (button.getName() == "snap" && on) {
        // segmented control: red text handled in drawButtonText via colour ids
    }
}

BeatManglerEditor::BeatManglerEditor(BeatManglerProcessor& p)
    : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&laf);
    setSize(640, 440);

    auto setupKnob = [&](juce::Slider& s, const juce::String& name,
                         const juce::String& param) {
        s.setName(name);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
        return param;
    };
    setupKnob(chaosKnob, "chaos", "chaos");
    setupKnob(densityKnob, "density", "density");
    setupKnob(pitchKnob, "pitch", "pitchTrim");
    setupKnob(degradeKnob, "degrade", "degradeTrim");

    auto& apvts = proc.apvts;
    chaosAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "chaos", chaosKnob);
    densityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "density", densityKnob);
    pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pitchTrim", pitchKnob);
    degradeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "degradeTrim", degradeKnob);

    auto setupLabel = [&](juce::Label& l, const juce::String& text, int size,
                          juce::Colour c = kText) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(size, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, c);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    titleLabel.setText("BEAT MANGLER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, kText);
    titleLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(titleLabel);
    setupLabel(subLabel, "v0.2  -  generative re-composer", 12, kDim);
    subLabel.setJustificationType(juce::Justification::right);
    setupLabel(chaosLabel, "CHAOS", 13, kRed);
    setupLabel(densityLabel, "DENSITY", 11);
    setupLabel(pitchLabel, "PITCH", 11);
    setupLabel(degradeLabel, "DEGRADE", 11);
    setupLabel(rateLabel, "RATE", 11);
    setupLabel(gridLabel, "GRID", 11);
    setupLabel(sliceInfoLabel, "no file loaded", 12, kDim);
    sliceInfoLabel.setJustificationType(juce::Justification::left);
    setupLabel(sliceReadout, "--", 22, kSteel);
    setupLabel(footerLabel, "garbage in, music out.", 12, kDim);

    fileLabel.setText(proc.loadedFileName(), juce::dontSendNotification);
    fileLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    fileLabel.setColour(juce::Label::textColourId, kDim);
    fileLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(fileLabel);

    const char* snapNames[3] = {"GLIDE", "SEMI", "SCALE"};
    for (int i = 0; i < 3; ++i) {
        snapButtons[i].setButtonText(snapNames[i]);
        snapButtons[i].setName("snap");
        snapButtons[i].setRadioGroupId(777);
        snapButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(snapButtons[i]);
        snapButtons[i].onClick = [this, i] {
            if (auto* param = proc.apvts.getParameter("pitchSnap"))
                param->setValueNotifyingHost((float)i / 2.0f);
        };
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            proc.apvts.getParameter("pitchSnap")))
        snapButtons[choice->getIndex()].setToggleState(
            true, juce::dontSendNotification);

    // RATE: 2x2 segmented (1/4, 1/2, 1, 2) in the old BPM knob slot
    const char* rateNames[4] = {"1/4", "1/2", "1", "2"};
    for (int i = 0; i < 4; ++i) {
        rateButtons[i].setButtonText(rateNames[i]);
        rateButtons[i].setName("snap");
        rateButtons[i].setRadioGroupId(778);
        rateButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(rateButtons[i]);
        rateButtons[i].onClick = [this, i] {
            if (auto* param = proc.apvts.getParameter("rate"))
                param->setValueNotifyingHost((float)i / 3.0f);
        };
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            proc.apvts.getParameter("rate")))
        rateButtons[choice->getIndex()].setToggleState(
            true, juce::dontSendNotification);

    // GRID: 16ths vs beats
    const char* gridNames[2] = {"16TH", "BEAT"};
    for (int i = 0; i < 2; ++i) {
        gridButtons[i].setButtonText(gridNames[i]);
        gridButtons[i].setName("snap");
        gridButtons[i].setRadioGroupId(779);
        gridButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(gridButtons[i]);
        gridButtons[i].onClick = [this, i] {
            if (auto* param = proc.apvts.getParameter("grid"))
                param->setValueNotifyingHost((float)i);
            // grid change requires re-slicing; reload the current file
            proc.resliceCurrentFile();
            updateSliceInfo();
        };
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            proc.apvts.getParameter("grid")))
        gridButtons[choice->getIndex()].setToggleState(
            true, juce::dontSendNotification);

    loadButton.onClick = [this] { loadFile(); };
    addAndMakeVisible(loadButton);

    playButton.setClickingTogglesState(true);
    playButton.onClick = [this] {
        proc.playing.store(playButton.getToggleState());
        playButton.setButtonText(playButton.getToggleState() ? "STOP" : "PLAY");
    };
    addAndMakeVisible(playButton);

    updateSliceInfo();
    startTimerHz(15);
}

void BeatManglerEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    // title bar strip
    g.setColour(kPanel);
    g.fillRect(0, 0, getWidth(), 64);
    g.setColour(juce::Colour(0xff232329));
    g.fillRect(0, 64, getWidth(), 1);
    // faint red baseline under the hero knob area
    g.setColour(juce::Colour(0x22ff3b30));
    g.fillRect(0, 268, getWidth(), 1);
}

void BeatManglerEditor::resized() {
    titleLabel.setBounds(24, 12, 400, 40);
    subLabel.setBounds(400, 24, 216, 20);
    chaosKnob.setBounds(36, 92, 150, 150);
    chaosLabel.setBounds(36, 244, 150, 20);
    densityKnob.setBounds(220, 100, 90, 90);
    densityLabel.setBounds(220, 192, 90, 18);
    pitchKnob.setBounds(320, 100, 90, 90);
    pitchLabel.setBounds(320, 192, 90, 18);
    degradeKnob.setBounds(420, 100, 90, 90);
    degradeLabel.setBounds(420, 192, 90, 18);
    // RATE 2x2 in the old BPM slot
    rateButtons[0].setBounds(524, 100, 43, 43);
    rateButtons[1].setBounds(569, 100, 43, 43);
    rateButtons[2].setBounds(524, 145, 43, 43);
    rateButtons[3].setBounds(569, 145, 43, 43);
    rateLabel.setBounds(524, 192, 90, 18);
    for (int i = 0; i < 3; ++i)
        snapButtons[i].setBounds(36 + i * 66, 286, 62, 34);
    gridButtons[0].setBounds(242, 286, 62, 34);
    gridButtons[1].setBounds(308, 286, 62, 34);
    gridLabel.setBounds(242, 322, 128, 14);
    loadButton.setBounds(392, 286, 90, 34);
    fileLabel.setBounds(492, 286, 124, 34);
    sliceInfoLabel.setBounds(36, 340, 200, 20);
    playButton.setBounds(252, 340, 130, 40);
    sliceReadout.setBounds(410, 340, 200, 40);
    footerLabel.setBounds(24, 398, 592, 24);
}

void BeatManglerEditor::loadFile() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load a break, beat, or any sample", juce::File(),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
                             auto f = fc.getResult();
                             if (f.existsAsFile()) {
                                 if (proc.loadAudioFile(f)) {
                                     fileLabel.setText(proc.loadedFileName(),
                                                       juce::dontSendNotification);
                                     updateSliceInfo();
                                 } else {
                                     fileLabel.setText("could not load file",
                                                       juce::dontSendNotification);
                                 }
                             }
                         });
}

void BeatManglerEditor::updateSliceInfo() {
    int n = proc.sliceCount.load();
    int bars = proc.barsCount.load();
    juce::String t;
    if (n <= 0) t = "no file loaded";
    else if (bars > 0) t = juce::String(bars) + " BARS - " + juce::String(n) + " SLICES";
    else t = juce::String(n) + " SLICES";
    sliceInfoLabel.setText(t, juce::dontSendNotification);
}

void BeatManglerEditor::timerCallback() {
    int ls = proc.lastSlice.load();
    int n = proc.sliceCount.load();
    sliceReadout.setText(n > 0 && ls >= 0
                             ? "SLICE " + juce::String(ls + 1).paddedLeft('0', 2)
                             : "--",
                         juce::dontSendNotification);
    if (!proc.playing.load() && playButton.getToggleState()) {
        playButton.setToggleState(false, juce::dontSendNotification);
        playButton.setButtonText("PLAY");
    }
    // keep segmented controls in sync with host/preset changes
    auto syncSeg = [&](juce::TextButton* btns, int count, const char* param) {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(
                proc.apvts.getParameter(param))) {
            const int idx = c->getIndex();
            for (int i = 0; i < count; ++i)
                btns[i].setToggleState(i == idx, juce::dontSendNotification);
        }
    };
    syncSeg(snapButtons, 3, "pitchSnap");
    syncSeg(rateButtons, 4, "rate");
    syncSeg(gridButtons, 2, "grid");
}
