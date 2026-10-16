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

// --- WaveformDisplay -------------------------------------------------------
WaveformDisplay::WaveformDisplay() { startTimerHz(30); }

void WaveformDisplay::setSnapshot(
    std::shared_ptr<const BreakManglerProcessor::WaveSnapshot> snap) {
    if (snap != snap_) {
        snap_ = snap;
        rebuildImage();
        dirty_ = true;
    }
}

void WaveformDisplay::setPlayState(int step, int totalSteps, int activeSlice,
                                   bool rolling) {
    if (step != step_ || totalSteps != totalSteps_ ||
        activeSlice != activeSlice_ || rolling != rolling_) {
        step_ = step;
        totalSteps_ = totalSteps;
        activeSlice_ = activeSlice;
        rolling_ = rolling;
        dirty_ = true;
    }
}

void WaveformDisplay::resized() {
    rebuildImage();
    dirty_ = true;
}

bool WaveformDisplay::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
            ext == ".flac" || ext == ".mp3")
            return true;
    }
    return false;
}

void WaveformDisplay::filesDropped(const juce::StringArray& files, int, int) {
    dragOver_ = false;
    repaint();
    for (auto& f : files) {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();
        if ((ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
             ext == ".flac" || ext == ".mp3") &&
            file.existsAsFile()) {
            if (onFileDrop) onFileDrop(file);
            return;
        }
    }
}

void WaveformDisplay::fileDragEnter(const juce::StringArray&, int, int) {
    dragOver_ = true;
    repaint();
}

void WaveformDisplay::fileDragExit(const juce::StringArray&) {
    dragOver_ = false;
    repaint();
}

void WaveformDisplay::timerCallback() {
    if (rolling_ || dirty_) {
        dirty_ = false;
        repaint();
    }
}

void WaveformDisplay::rebuildImage() {
    const int W = juce::jmax(32, getWidth());
    const int H = juce::jmax(16, getHeight());
    waveImage_ = juce::Image(juce::Image::ARGB, W, H, true);
    juce::Graphics g(waveImage_);
    if (!snap_ || snap_->totalSamples <= 0 ||
        snap_->mins.size() != snap_->maxs.size()) {
        return; // placeholder text is drawn in paint()
    }
    const int cols = (int)snap_->mins.size();
    const float mid = H * 0.5f;
    const float amp = H * 0.46f;
    g.setColour(juce::Colour(0xff9aa0a8)); // steel waveform
    for (int x = 0; x < W; ++x) {
        const int c = juce::jmin(cols - 1, (x * cols) / W);
        const float y1 = mid - snap_->maxs[(size_t)c] * amp;
        const float y2 = mid - snap_->mins[(size_t)c] * amp;
        g.drawLine((float)x, y1, (float)x, juce::jmax(y1 + 1.0f, y2), 1.0f);
    }
    // faint center line
    g.setColour(juce::Colour(0x22ffffff));
    g.drawLine(0.0f, mid, (float)W, mid, 1.0f);
}

void WaveformDisplay::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const float W = bounds.getWidth(), H = bounds.getHeight();
    g.setColour(juce::Colour(0xff101014));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2a2a30));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    if (dragOver_) {
        g.setColour(juce::Colour(0xaaff3b30));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 2.0f);
    }

    if (!snap_ || snap_->totalSamples <= 0 ||
        snap_->sliceBounds.empty()) {
        g.setColour(juce::Colour(0xff7a7a84));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("load a break", bounds,
                   juce::Justification::centred, false);
        return;
    }
    g.drawImage(waveImage_, bounds);

    const double total = (double)snap_->totalSamples;
    const int ns = (int)snap_->sliceBounds.size();
    // slice grid lines
    g.setColour(juce::Colour(0xff2e2e35));
    for (const auto& b : snap_->sliceBounds) {
        const float x = (float)(b.first / total) * W;
        g.drawLine(x, 0.0f, x, H, 1.0f);
    }
    // bar lines: a brighter line every bar's worth of slices
    if (snap_->bars > 0 && ns >= snap_->bars) {
        const int per = ns / snap_->bars;
        g.setColour(juce::Colour(0xff4a4a55));
        for (int i = 0; i <= snap_->bars; ++i) {
            const int si = juce::jmin(ns - 1, i * per);
            const float x = (float)(snap_->sliceBounds[(size_t)si].first / total) * W;
            g.drawLine(x, 0.0f, x, H, 1.0f);
        }
    }
    // glowing active slice
    if (rolling_ && activeSlice_ >= 0 && activeSlice_ < ns) {
        const auto& b = snap_->sliceBounds[(size_t)activeSlice_];
        const float x0 = (float)(b.first / total) * W;
        const float x1 = (float)(b.second / total) * W;
        juce::Rectangle<float> r(x0, 0.0f, juce::jmax(2.0f, x1 - x0), H);
        g.setColour(juce::Colour(0x30ff3b30));
        g.fillRect(r.expanded(3.0f, 0.0f));
        g.setColour(juce::Colour(0x66ff3b30));
        g.fillRect(r);
        g.setColour(juce::Colour(0xffff3b30));
        g.drawRect(r, 1.5f);
    }
    // playhead: linear position across the bar grid
    if (rolling_ && totalSteps_ > 0) {
        const float x =
            juce::jlimit(0.0f, W - 1.0f, (float)step_ / (float)totalSteps_ * W);
        g.setColour(juce::Colour(0xffff3b30));
        g.drawLine(x, 0.0f, x, H, 2.0f);
    }
}

BreakManglerEditor::BreakManglerEditor(BreakManglerProcessor& p)
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
    setupKnob(eraKnob, "era", "era");

    auto& apvts = proc.apvts;
    chaosAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "chaos", chaosKnob);
    densityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "density", densityKnob);
    pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pitchTrim", pitchKnob);
    degradeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "degradeTrim", degradeKnob);
    eraAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "era", eraKnob);

    auto setupLabel = [&](juce::Label& l, const juce::String& text, int size,
                          juce::Colour c = kText) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(size, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, c);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    titleLabel.setText("BREAK MANGLER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, kText);
    titleLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(titleLabel);
    setupLabel(subLabel, "v0.12  -  generative re-composer", 12, kDim);
    subLabel.setJustificationType(juce::Justification::right);
    setupLabel(chaosLabel, "CHAOS", 13, kRed);
    setupLabel(densityLabel, "DENSITY", 11);
    setupLabel(pitchLabel, "PITCH", 11);
    setupLabel(degradeLabel, "DEGRADE", 11);
    setupLabel(rateLabel, "RATE", 11);
    setupLabel(gridLabel, "GRID", 11);
    setupLabel(snapLabel, "SNAP", 11);
    setupLabel(eraLabel, "ERA", 11);
    setupLabel(sliceInfoLabel, "no file loaded", 12, kDim);
    sliceInfoLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(waveform);
    waveform.onFileDrop = [this](const juce::File& f) {
        if (proc.loadAudioFile(f)) {
            fileLabel.setText(proc.loadedFileName(),
                               juce::dontSendNotification);
            updateSliceInfo();
        } else {
            fileLabel.setText("could not load file",
                               juce::dontSendNotification);
        }
    };

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
            if (proc.autoBypass.load())
                proc.manSnap.store(i);
            else if (auto* param = proc.apvts.getParameter("pitchSnap"))
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
            static constexpr float vals[4] = {0.25f, 0.5f, 1.0f, 2.0f};
            if (proc.autoBypass.load())
                proc.manRate.store(vals[i]);
            else if (auto* param = proc.apvts.getParameter("rate"))
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
            if (proc.autoBypass.load())
                proc.manGrid.store(i == 1 ? 4 : 16);
            else if (auto* param = proc.apvts.getParameter("grid"))
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

    // Presets live in the header: factory set plus the user's own.
    refreshPresetList();
    addAndMakeVisible(presetBox);
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        const auto factories = BreakManglerProcessor::factoryPresets();
        if (id <= (int)factories.size()) {
            proc.applyPreset(factories[id - 1]);
        } else {
            proc.loadUserPreset(presetBox.getText());
        }
        updateSliceInfo();
    };
    presetSave.onClick = [this] {
        auto* alert = new juce::AlertWindow(
            "Save preset", "Name this preset:", juce::AlertWindow::NoIcon);
        alert->addTextEditor("name", "", "Preset name:");
        alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        alert->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, alert](int result) {
                if (result == 1) {
                    const juce::String name =
                        alert->getTextEditorContents("name");
                    if (proc.saveUserPreset(name)) {
                        refreshPresetList();
                        for (int i = 0; i < presetBox.getNumItems(); ++i) {
                            if (presetBox.getItemText(i) == name) {
                                presetBox.setSelectedItemIndex(
                                    i, juce::dontSendNotification);
                                break;
                            }
                        }
                    }
                }
            }), true); // deletes itself when dismissed
    };
    presetDel.onClick = [this] {
        const int id = presetBox.getSelectedId();
        const auto factories = BreakManglerProcessor::factoryPresets();
        if (id > (int)factories.size() &&
            proc.deleteUserPreset(presetBox.getText()))
            refreshPresetList();
    };
    addAndMakeVisible(presetSave);
    addAndMakeVisible(presetDel);

    // FIT: load-time fit mode. VARISPEED repitches; S950 keeps the pitch
    // and fits time with repeated/dropped segments (grit baked in).
    fitButton.setClickingTogglesState(true);
    fitButton.setToggleState(false, juce::dontSendNotification);
    fitButton.onClick = [this] {
        const bool s950 = fitButton.getToggleState();
        if (proc.autoBypass.load()) {
            proc.manFitMode.store(s950 ? 1 : 0);
        } else if (auto* param = proc.apvts.getParameter("fitMode")) {
            param->setValueNotifyingHost(s950 ? 1.0f : 0.0f);
        }
        fitButton.setButtonText(s950 ? "FIT: S950" : "FIT: VARISPEED");
        proc.resliceCurrentFile();
        updateSliceInfo();
    };
    addAndMakeVisible(fitButton);

    playButton.setClickingTogglesState(true);
    playButton.setToggleState(true, juce::dontSendNotification);
    playButton.onClick = [this] {
        const bool open = playButton.getToggleState();
        proc.gateOpen.store(open);
        // optimistic label; the timer refines it (QUEUED while armed)
        playButton.setButtonText(open ? "QUEUED" : "PLAY");
    };
    addAndMakeVisible(playButton);

    // Automation bypass toggle: HOST = params follow the DAW (incl. any
    // envelopes); MANUAL = engine runs on a snapshot you can tweak freely.
    autoButton.setClickingTogglesState(true);
    autoButton.onClick = [this] { setAutoBypass(autoButton.getToggleState(), true); };
    addAndMakeVisible(autoButton);

    // While bypassed, knob turns write the manual snapshot directly.
    chaosKnob.onValueChange = [this] {
        if (proc.autoBypass.load()) proc.manChaos.store((float)chaosKnob.getValue());
    };
    densityKnob.onValueChange = [this] {
        if (proc.autoBypass.load()) proc.manDensity.store((float)densityKnob.getValue());
    };
    pitchKnob.onValueChange = [this] {
        if (proc.autoBypass.load()) proc.manPitch.store((float)pitchKnob.getValue());
    };
    degradeKnob.onValueChange = [this] {
        if (proc.autoBypass.load()) proc.manDegrade.store((float)degradeKnob.getValue());
    };
    eraKnob.onValueChange = [this] {
        if (proc.autoBypass.load()) proc.manEra.store((float)eraKnob.getValue());
    };

    setupLabel(transportLabel, "HOST STOPPED", 11, kDim);
    transportLabel.setJustificationType(juce::Justification::left);

    // If the project was saved with automation bypassed, open in that mode
    // (keeping the restored manual values, no fresh snapshot).
    if (proc.autoBypass.load()) {
        autoButton.setToggleState(true, juce::dontSendNotification);
        setAutoBypass(true, false);
    }

    updateSliceInfo();
    startTimerHz(15);
}

void BreakManglerEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    // title bar strip
    g.setColour(kPanel);
    g.fillRect(0, 0, getWidth(), 64);
    g.setColour(juce::Colour(0xff232329));
    g.fillRect(0, 64, getWidth(), 1);
    // faint red baseline under the hero knob area
    g.setColour(juce::Colour(0x22ff3b30));
    g.fillRect(0, 268, getWidth(), 1);
    // thin group line tying the SNAP buttons together
    g.setColour(juce::Colour(0xff2a2a30));
    g.drawLine(36.0f, 320.0f, 230.0f, 320.0f, 1.0f);
}

void BreakManglerEditor::resized() {
    titleLabel.setBounds(24, 12, 400, 40);
    subLabel.setBounds(400, 24, 216, 20);
    chaosKnob.setBounds(30, 96, 140, 140);
    chaosLabel.setBounds(30, 238, 140, 18);
    densityKnob.setBounds(184, 104, 78, 78);
    densityLabel.setBounds(184, 184, 78, 16);
    pitchKnob.setBounds(272, 104, 78, 78);
    pitchLabel.setBounds(272, 184, 78, 16);
    degradeKnob.setBounds(360, 104, 78, 78);
    degradeLabel.setBounds(360, 184, 78, 16);
    eraKnob.setBounds(448, 104, 78, 78);
    eraLabel.setBounds(448, 184, 78, 16);
    // RATE 2x2, slightly smaller to make room for ERA
    rateButtons[0].setBounds(540, 104, 38, 38);
    rateButtons[1].setBounds(582, 104, 38, 38);
    rateButtons[2].setBounds(540, 146, 38, 38);
    rateButtons[3].setBounds(582, 146, 38, 38);
    rateLabel.setBounds(540, 186, 80, 16);
    for (int i = 0; i < 3; ++i)
        snapButtons[i].setBounds(36 + i * 66, 286, 62, 34);
    gridButtons[0].setBounds(242, 286, 62, 34);
    gridButtons[1].setBounds(308, 286, 62, 34);
    gridLabel.setBounds(242, 322, 128, 14);
    snapLabel.setBounds(36, 322, 194, 14);
    loadButton.setBounds(392, 286, 90, 34);
    fileLabel.setBounds(492, 286, 124, 34);
    sliceInfoLabel.setBounds(36, 340, 200, 20);
    transportLabel.setBounds(36, 366, 200, 16);
    playButton.setBounds(252, 340, 130, 40);
    waveform.setBounds(392, 336, 224, 88);
    autoButton.setBounds(252, 392, 130, 30);
    fitButton.setBounds(36, 392, 140, 28);
    presetBox.setBounds(400, 42, 150, 20);
    presetSave.setBounds(554, 42, 36, 20);
    presetDel.setBounds(594, 42, 36, 20);
}

void BreakManglerEditor::loadFile() {
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

void BreakManglerEditor::refreshPresetList() {
    presetBox.clear(juce::dontSendNotification);
    const auto factories = BreakManglerProcessor::factoryPresets();
    int id = 1;
    for (auto& p : factories)
        presetBox.addItem(p.name, id++);
    const auto users = proc.userPresetNames();
    if (!users.empty()) {
        presetBox.addSeparator();
        for (auto& n : users)
            presetBox.addItem(n, id++);
    }
    presetBox.setText("Presets", juce::dontSendNotification);
}

void BreakManglerEditor::updateSliceInfo() {
    int n = proc.sliceCount.load();
    int bars = proc.barsCount.load();
    juce::String t;
    if (n <= 0) t = "no file loaded";
    else if (bars > 0) t = juce::String(bars) + " BARS - " + juce::String(n) + " SLICES";
    else t = juce::String(n) + " SLICES";
    sliceInfoLabel.setText(t, juce::dontSendNotification);
}

void BreakManglerEditor::setAutoBypass(bool on, bool snapshot) {
    if (on) {
        if (snapshot) proc.engageAutoBypass(); // else: keep restored values
        // Detach knobs from the host-driven params so envelopes stop
        // yanking them; they now edit the manual snapshot (onValueChange).
        chaosAtt.reset();
        densityAtt.reset();
        pitchAtt.reset();
        degradeAtt.reset();
        eraAtt.reset();
        chaosKnob.setValue(proc.manChaos.load(), juce::dontSendNotification);
        densityKnob.setValue(proc.manDensity.load(), juce::dontSendNotification);
        pitchKnob.setValue(proc.manPitch.load(), juce::dontSendNotification);
        degradeKnob.setValue(proc.manDegrade.load(), juce::dontSendNotification);
        eraKnob.setValue(proc.manEra.load(), juce::dontSendNotification);
        autoButton.setButtonText("AUTO: MANUAL");
    } else {
        proc.autoBypass.store(false);
        // Re-attach: knobs resync to the host params (and any envelopes).
        chaosAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "chaos", chaosKnob);
        densityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "density", densityKnob);
        pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "pitchTrim", pitchKnob);
        degradeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "degradeTrim", degradeKnob);
        eraAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "era", eraKnob);
        autoButton.setButtonText("AUTO: HOST");
    }
}

juce::String BreakManglerEditor::mangleWord(float intensity, juce::Random& rng) {
    static const char truth[8] = "MANGLER";
    static const char glyphs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%&@$?!*+=";
    const int ng = (int)sizeof(glyphs) - 1;
    char out[8];
    for (int i = 0; i < 7; ++i)
        out[i] = (rng.nextFloat() < intensity) ? glyphs[rng.nextInt(ng)] : truth[i];
    out[7] = '\0';
    return juce::String(out);
}

void BreakManglerEditor::timerCallback() {
    // project restore: the sample (re)slices on the first audio block —
    // if the count or filename changes under us, refresh the readouts
    const int scNow = proc.sliceCount.load();
    if (scNow != shownSlices_) {
        shownSlices_ = scNow;
        updateSliceInfo();
    }
    if (fileLabel.getText() != proc.loadedFileName())
        fileLabel.setText(proc.loadedFileName(), juce::dontSendNotification);

    int ls = proc.lastSlice.load();
    int n = proc.sliceCount.load();
    const bool tp = proc.transportPlaying.load();
    transportLabel.setText(tp ? "HOST PLAYING" : "HOST STOPPED",
                           juce::dontSendNotification);
    transportLabel.setColour(juce::Label::textColourId, tp ? kRed : kDim);
    // play button reflects the gate/launch state
    if (!playButton.getToggleState())
        playButton.setButtonText("PLAY");
    else if (proc.launchArmed.load())
        playButton.setButtonText("QUEUED");
    else
        playButton.setButtonText("STOP");
    // Scramble display: while the engine is actually playing, the word
    // MANGLER mangles itself on every slice hit (harder with more CHAOS)
    // and wrong letters heal back toward the truth between hits.
    // Stopped = clean title. The waveform screen shows slice activity.
    {
        const bool rolling = tp && playButton.getToggleState() && n > 0;
        if (rolling) {
            const float chaos = proc.effectiveParams().chaos;
            if (ls != lastSliceSeen_) {
                manglerShown_ = mangleWord(0.2f + 0.8f * chaos, rng_).toStdString();
            } else {
                static const char truth[8] = "MANGLER";
                for (int i = 0; i < 7; ++i)
                    if (manglerShown_[(size_t)i] != truth[i] &&
                        rng_.nextFloat() < 0.18f)
                        manglerShown_[(size_t)i] = truth[i];
            }
        } else {
            manglerShown_ = "MANGLER";
        }
        lastSliceSeen_ = ls;
        titleLabel.setText("BREAK " + juce::String(manglerShown_),
                           juce::dontSendNotification);
        waveform.setSnapshot(proc.waveSnapshot());
        waveform.setPlayState(proc.playheadStep.load(), n,
                              rolling ? ls : -1, rolling);
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
    if (proc.autoBypass.load()) {
        // segmented controls follow the manual snapshot instead
        auto setSeg = [&](juce::TextButton* btns, int count, int idx) {
            for (int i = 0; i < count; ++i)
                btns[i].setToggleState(i == idx, juce::dontSendNotification);
        };
        static constexpr float rateVals[4] = {0.25f, 0.5f, 1.0f, 2.0f};
        int rIdx = 2;
        for (int i = 0; i < 4; ++i)
            if (proc.manRate.load() == rateVals[i]) rIdx = i;
        setSeg(snapButtons, 3, proc.manSnap.load());
        setSeg(rateButtons, 4, rIdx);
        setSeg(gridButtons, 2, proc.manGrid.load() == 4 ? 1 : 0);
    } else {
        syncSeg(snapButtons, 3, "pitchSnap");
        syncSeg(rateButtons, 4, "rate");
        syncSeg(gridButtons, 2, "grid");
    }
    // FIT toggle follows the fitMode param (or the manual snapshot)
    {
        const bool s950 = proc.autoBypass.load()
            ? proc.manFitMode.load() == 1
            : proc.effectiveParams().fitMode == 1;
        fitButton.setToggleState(s950, juce::dontSendNotification);
        fitButton.setButtonText(s950 ? "FIT: S950" : "FIT: VARISPEED");
    }
}
