#include "PluginEditor.h"

namespace sapporch {

namespace {

juce::String midiNoteName(int note)
{
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[((note % 12) + 12) % 12]) + juce::String(note / 12 - 1);
}

juce::Font titleFont(float height)
{
    return juce::Font(juce::FontOptions{"Georgia", height, juce::Font::plain});
}
juce::Font uiFont(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions{height, bold ? juce::Font::bold : juce::Font::plain});
}

} // namespace

// ------------------------------------------------------------ look and feel --

OrchestraLookAndFeel::OrchestraLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, palette::dim);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, palette::ivory);
    setColour(juce::ComboBox::backgroundColourId, palette::panel);
    setColour(juce::ComboBox::textColourId, palette::ivory);
    setColour(juce::ComboBox::outlineColourId, palette::panelEdge);
    setColour(juce::ComboBox::arrowColourId, palette::gold);
    setColour(juce::PopupMenu::backgroundColourId, palette::panel);
    setColour(juce::PopupMenu::textColourId, palette::ivory);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette::burgundy);
    setColour(juce::TextButton::buttonColourId, palette::panel);
    setColour(juce::TextButton::textColourOffId, palette::ivory);
    setColour(juce::TextButton::textColourOnId, palette::background);
    setColour(juce::ToggleButton::textColourId, palette::dim);
    setColour(juce::ToggleButton::tickColourId, palette::gold);
    setColour(juce::ToggleButton::tickDisabledColourId, palette::panelEdge);
}

void OrchestraLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                            float sliderPos, float startAngle,
                                            float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcThickness = juce::jmax(2.4f, radius * 0.085f);
    const float arcRadius = radius - arcThickness * 0.5f;

    // Body: soft radial cap.
    const float capRadius = arcRadius - arcThickness * 1.7f;
    juce::ColourGradient bodyGrad(palette::panelEdge.brighter(0.12f),
                                  centre.x - capRadius * 0.4f, centre.y - capRadius * 0.5f,
                                  palette::shadow, centre.x, centre.y + capRadius, true);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2, capRadius * 2);
    g.setColour(palette::shadow.withAlpha(0.6f));
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2, capRadius * 2, 1.0f);

    // Track arc.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(palette::shadow);
    g.strokePath(track, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Value arc with a gold glow.
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        startAngle, angle, true);
    g.setColour(palette::gold.withAlpha(0.25f));
    g.strokePath(value, juce::PathStrokeType(arcThickness * 2.2f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour(slider.isEnabled() ? palette::goldBright : palette::dim);
    g.strokePath(value, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer.
    juce::Path pointer;
    pointer.addRoundedRectangle(-arcThickness * 0.5f, -capRadius + arcThickness,
                                arcThickness, capRadius * 0.42f, arcThickness * 0.4f);
    g.setColour(palette::goldBright);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void OrchestraLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0, 0, float(width), float(height)).reduced(0.5f);
    g.setColour(palette::panel);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(box.hasKeyboardFocus(true) ? palette::gold : palette::panelEdge);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = float(width) - 14.0f, ay = float(height) * 0.5f;
    arrow.addTriangle(ax - 4, ay - 2.5f, ax + 4, ay - 2.5f, ax, ay + 3.5f);
    g.setColour(palette::gold);
    g.fillPath(arrow);
}

void OrchestraLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool highlighted,
                                                bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    juce::Colour fill = on ? palette::gold : palette::panel;
    if (down) fill = fill.darker(0.2f);
    else if (highlighted) fill = fill.brighter(0.08f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(on ? palette::goldBright : palette::panelEdge);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

juce::Font OrchestraLookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.0f); }
juce::Font OrchestraLookAndFeel::getPopupMenuFont() { return uiFont(13.5f); }

// -------------------------------------------------------------------- knob ---

Knob::Knob(juce::AudioProcessorValueTreeState&, const juce::String&,
           const juce::String& title, bool big)
    : big_(big)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    addAndMakeVisible(slider);

    label_.setText(title, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(uiFont(big ? 13.0f : 11.0f, big));
    label_.setColour(juce::Label::textColourId, big ? palette::ivory : palette::dim);
    label_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label_);
}

void Knob::resized()
{
    auto bounds = getLocalBounds();
    label_.setBounds(bounds.removeFromBottom(big_ ? 18 : 14));
    slider.setBounds(bounds);
}

// --------------------------------------------------------------- stage pad ---

StagePad::StagePad(juce::AudioProcessorValueTreeState& state)
    : state_(state),
      xParam_(state.getParameter("stageX")),
      yParam_(state.getParameter("stageDepth"))
{
    startTimerHz(30);
}
StagePad::~StagePad() { stopTimer(); }

void StagePad::timerCallback() { repaint(); }

void StagePad::applyPoint(juce::Point<float> p)
{
    const auto area = getLocalBounds().toFloat().reduced(10.0f);
    const float nx = juce::jlimit(0.0f, 1.0f, (p.x - area.getX()) / area.getWidth());
    const float ny = juce::jlimit(0.0f, 1.0f, (p.y - area.getY()) / area.getHeight());
    xParam_->setValueNotifyingHost(nx);
    yParam_->setValueNotifyingHost(1.0f - ny);  // top of pad = back of stage
}

void StagePad::mouseDown(const juce::MouseEvent& e)
{
    xParam_->beginChangeGesture();
    yParam_->beginChangeGesture();
    applyPoint(e.position);
    glow_ = 1.0f;
}
void StagePad::mouseDrag(const juce::MouseEvent& e) { applyPoint(e.position); }
void StagePad::mouseUp(const juce::MouseEvent&)
{
    xParam_->endChangeGesture();
    yParam_->endChangeGesture();
}

void StagePad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(palette::shadow);
    g.fillRoundedRectangle(bounds, 6.0f);

    const auto area = bounds.reduced(10.0f);

    // Stage floor: concentric seating arcs radiating from the podium (bottom centre).
    const juce::Point<float> podium(area.getCentreX(), area.getBottom() + 6.0f);
    g.setColour(palette::panelEdge.withAlpha(0.85f));
    for (int row = 1; row <= 5; ++row) {
        const float r = area.getHeight() * (0.22f + 0.18f * float(row));
        juce::Path arc;
        arc.addCentredArc(podium.x, podium.y, r, r * 0.78f, 0.0f,
                          -juce::MathConstants<float>::pi * 0.42f,
                          juce::MathConstants<float>::pi * 0.42f, true);
        g.strokePath(arc, juce::PathStrokeType(1.0f));
    }
    // Centre line.
    g.setColour(palette::panelEdge.withAlpha(0.5f));
    g.drawVerticalLine(int(area.getCentreX()), area.getY(), area.getBottom());

    // Podium marker.
    g.setColour(palette::dim.withAlpha(0.8f));
    g.fillEllipse(podium.x - 3.0f, area.getBottom() - 4.0f, 6.0f, 6.0f);

    // Source position.
    const float nx = xParam_->getValue();
    const float ny = 1.0f - yParam_->getValue();
    const juce::Point<float> pos(area.getX() + nx * area.getWidth(),
                                 area.getY() + ny * area.getHeight());
    glow_ = juce::jmax(0.45f, glow_ * 0.94f);
    g.setColour(palette::gold.withAlpha(0.18f * glow_ + 0.12f));
    g.fillEllipse(pos.x - 14, pos.y - 14, 28, 28);
    g.setColour(palette::gold.withAlpha(0.35f));
    g.fillEllipse(pos.x - 8, pos.y - 8, 16, 16);
    g.setColour(palette::goldBright);
    g.fillEllipse(pos.x - 4, pos.y - 4, 8, 8);

    g.setColour(palette::dim);
    g.setFont(uiFont(9.5f));
    g.drawText("BACK", area.toNearestInt().removeFromTop(12), juce::Justification::centredTop);
    g.drawText("PODIUM", getLocalBounds().removeFromBottom(13),
               juce::Justification::centredBottom);
}

// ---------------------------------------------------------------- keyboard ---

OrchestraKeyboard::OrchestraKeyboard(SappOrchestraProcessor& processor,
                                     juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard),
      processor_(processor)
{
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe4dccb));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff26211b));
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff9b917f));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              palette::gold.withAlpha(0.35f));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              palette::gold.withAlpha(0.6f));
    setColour(juce::MidiKeyboardComponent::shadowColourId, palette::shadow.withAlpha(0.7f));
    setAvailableRange(12, 108);
    setKeyWidth(13.0f);
    setScrollButtonsVisible(false);
}

bool OrchestraKeyboard::keyswitchInfo(int note, bool& isActive) const
{
    isActive = false;
    auto inst = processor_.engine().currentInstrument();
    if (!inst) return false;
    const auto& def = inst->definition;
    if (def.keyswitchLo < 0 || note < def.keyswitchLo || note > def.keyswitchHi)
        return false;
    sapp::sounds::DiagnosticSnapshot snap;
    if (processor_.engine().sampler().diagnostics().read(snap))
        isActive = snap.activeKeyswitch == note;
    return true;
}

void OrchestraKeyboard::drawWhiteNote(int note, juce::Graphics& g,
                                      juce::Rectangle<float> area, bool isDown,
                                      bool isOver, juce::Colour lineColour,
                                      juce::Colour textColour)
{
    bool active = false;
    if (keyswitchInfo(note, active)) {
        g.setColour(active ? palette::gold : palette::burgundy.withAlpha(0.85f));
        g.fillRect(area);
        if (isDown) { g.setColour(palette::goldBright.withAlpha(0.5f)); g.fillRect(area); }
        g.setColour(lineColour);
        g.fillRect(area.withWidth(1.0f));
        return;
    }
    juce::MidiKeyboardComponent::drawWhiteNote(note, g, area, isDown, isOver,
                                               lineColour, textColour);
}

void OrchestraKeyboard::drawBlackNote(int note, juce::Graphics& g,
                                      juce::Rectangle<float> area, bool isDown,
                                      bool isOver, juce::Colour fill)
{
    bool active = false;
    if (keyswitchInfo(note, active)) {
        g.setColour(active ? palette::gold : palette::burgundy);
        g.fillRect(area);
        if (isDown) { g.setColour(palette::goldBright.withAlpha(0.5f)); g.fillRect(area); }
        return;
    }
    juce::MidiKeyboardComponent::drawBlackNote(note, g, area, isDown, isOver, fill);
}

// ------------------------------------------------------------------- editor --

SappOrchestraEditor::SappOrchestraEditor(SappOrchestraProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor)
{
    setLookAndFeel(&lookAndFeel_);

    auto& state = processor_.valueTree();

    title_.setText("SappOrchestra", juce::dontSendNotification);
    title_.setFont(titleFont(30.0f));
    title_.setColour(juce::Label::textColourId, palette::goldBright);
    addAndMakeVisible(title_);

    subtitle_.setText("ORCHESTRAL ENGINE", juce::dontSendNotification);
    subtitle_.setFont(uiFont(10.0f));
    subtitle_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(subtitle_);

    instrumentName_.setFont(uiFont(15.0f, true));
    instrumentName_.setColour(juce::Label::textColourId, palette::ivory);
    instrumentName_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(instrumentName_);

    status_.setFont(uiFont(11.0f));
    status_.setColour(juce::Label::textColourId, palette::dim);
    status_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status_);

    loadButton_.onClick = [this] { chooseSfz(); };
    addAndMakeVisible(loadButton_);
    diagButton_.onClick = [this] { processor_.loadDiagnosticInstrument(); };
    addAndMakeVisible(diagButton_);

    auto header = [&](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(uiFont(10.5f, true));
        label.setColour(juce::Label::textColourId, palette::dim);
        addAndMakeVisible(label);
    };
    header(articulationsHeader_, "ARTICULATIONS");
    header(stageHeader_, "STAGE");
    header(hallHeader_, "HALL");
    header(toneHeader_, "PERFORMANCE");

    auto knob = [&](const juce::String& id, const juce::String& text, bool big = false) {
        auto k = std::make_unique<Knob>(state, id, text, big);
        sliderAttachments_.push_back(
            std::make_unique<SliderAttachment>(state, id, k->slider));
        addAndMakeVisible(*k);
        return k;
    };
    dynamics_ = knob("dynamics", "DYNAMICS", true);
    expression_ = knob("expression", "EXPRESSION", true);
    width_ = knob("width", "WIDTH");
    dnaAmount_ = knob("dnaAmount", "DNA");
    hallSize_ = knob("hallSize", "SIZE");
    hallDecay_ = knob("hallDecay", "DECAY");
    hallDamping_ = knob("hallDamping", "DAMP");
    early_ = knob("earlyLevel", "EARLY");
    tail_ = knob("tailLevel", "TAIL");
    master_ = knob("masterGain", "MASTER");

    dnaMode_.addItemList({"Clean", "Cohesive", "Vintage"}, 1);
    dnaModeAttachment_ = std::make_unique<ComboAttachment>(state, "dnaMode", dnaMode_);
    addAndMakeVisible(dnaMode_);

    quality_.addItemList({"Draft", "Normal"}, 1);
    qualityAttachment_ = std::make_unique<ComboAttachment>(state, "quality", quality_);
    addAndMakeVisible(quality_);

    limiterAttachment_ = std::make_unique<ButtonAttachment>(state, "limiter", limiter_);
    addAndMakeVisible(limiter_);

    stagePad_ = std::make_unique<StagePad>(state);
    addAndMakeVisible(*stagePad_);

    keyboard_ = std::make_unique<OrchestraKeyboard>(processor_, processor_.keyboardState);
    addAndMakeVisible(*keyboard_);

    voicesLabel_.setFont(uiFont(11.0f));
    voicesLabel_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(voicesLabel_);

    processor_.onInstrumentChanged = [this] { rebuildArticulationChips(); };
    rebuildArticulationChips();

    startTimerHz(24);
    setResizable(true, true);
    setResizeLimits(760, 500, 1600, 1050);
    getConstrainer()->setFixedAspectRatio(920.0 / 600.0);
    setSize(920, 600);
}

SappOrchestraEditor::~SappOrchestraEditor()
{
    processor_.onInstrumentChanged = nullptr;
    setLookAndFeel(nullptr);
}

void SappOrchestraEditor::chooseSfz()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load SFZ instrument", juce::File(), "*.sfz");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                      processor_.loadSfzInstrument(file);
                              });
}

void SappOrchestraEditor::rebuildArticulationChips()
{
    articulationChips_.clear();
    const auto names = processor_.articulationNames();
    auto inst = processor_.engine().currentInstrument();
    for (int i = 0; i < names.size(); ++i) {
        auto* chip = articulationChips_.add(new juce::TextButton());
        juce::String text = names[i];
        if (inst && size_t(i) < inst->definition.articulations.size()) {
            const int ks = inst->definition.articulations[size_t(i)].keyswitch;
            if (ks >= 0) text << "   " << midiNoteName(ks);
        }
        chip->setButtonText(text);
        chip->setClickingTogglesState(false);
        chip->onClick = [this, i] { processor_.selectArticulation(i); };
        addAndMakeVisible(chip);
    }
    instrumentName_.setText(processor_.currentInstrumentName(), juce::dontSendNotification);
    resized();
    repaint();
}

void SappOrchestraEditor::timerCallback()
{
    status_.setText(processor_.loadStatus(), juce::dontSendNotification);
    instrumentName_.setText(processor_.currentInstrumentName(), juce::dontSendNotification);

    sapp::sounds::DiagnosticSnapshot snap;
    if (processor_.engine().sampler().diagnostics().read(snap)) {
        voicesLabel_.setText(juce::String(snap.activeVoices) + " voices",
                             juce::dontSendNotification);
        meterL_ = juce::jmax(snap.lastPeakL, meterL_ * 0.86f);
        meterR_ = juce::jmax(snap.lastPeakR, meterR_ * 0.86f);

        // Reflect the sounding articulation in the chip states.
        if (auto inst = processor_.engine().currentInstrument()) {
            const auto& arts = inst->definition.articulations;
            for (int i = 0; i < articulationChips_.size() && size_t(i) < arts.size(); ++i)
                articulationChips_[i]->setToggleState(
                    arts[size_t(i)].keyswitch == snap.activeKeyswitch &&
                        snap.activeKeyswitch >= 0,
                    juce::dontSendNotification);
        }
    }
    keyboard_->repaint();
    repaint(meterArea_);
}

void SappOrchestraEditor::paint(juce::Graphics& g)
{
    // Warm charcoal with a gentle top-light and vignette.
    juce::ColourGradient grad(palette::background.brighter(0.06f), 0.0f, 0.0f,
                              palette::background.darker(0.25f), 0.0f, float(getHeight()),
                              false);
    g.setGradientFill(grad);
    g.fillAll();

    // Panels.
    auto panel = [&](juce::Rectangle<int> r) {
        g.setColour(palette::panel.withAlpha(0.75f));
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(palette::panelEdge);
        g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);
    };
    const float scale = float(getWidth()) / 920.0f;
    auto s = [scale](int v) { return int(float(v) * scale); };

    panel({s(14), s(74), s(196), s(360)});    // articulations
    panel({s(220), s(74), s(330), s(360)});   // performance
    panel({s(560), s(74), s(346), s(360)});   // stage + hall

    // Gold hairline under the header.
    g.setColour(palette::gold.withAlpha(0.35f));
    g.fillRect(s(14), s(66), getWidth() - s(28), 1);

    // Peak meter.
    if (!meterArea_.isEmpty()) {
        g.setColour(palette::shadow);
        g.fillRoundedRectangle(meterArea_.toFloat(), 3.0f);
        auto bar = [&](float level, juce::Rectangle<int> r) {
            const float db = juce::Decibels::gainToDecibels(level, -60.0f);
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto fill = r.toFloat();
            fill = fill.removeFromLeft(fill.getWidth() * norm);
            g.setColour(db > -3.0f ? palette::burgundy.brighter(0.3f) : palette::gold);
            g.fillRoundedRectangle(fill, 2.0f);
        };
        auto inner = meterArea_.reduced(2);
        bar(meterL_, inner.removeFromTop(inner.getHeight() / 2).reduced(0, 1));
        bar(meterR_, inner.reduced(0, 1));
    }
}

void SappOrchestraEditor::resized()
{
    const float scale = float(getWidth()) / 920.0f;
    auto s = [scale](int v) { return int(float(v) * scale); };

    title_.setBounds(s(18), s(10), s(260), s(34));
    subtitle_.setBounds(s(21), s(42), s(260), s(16));
    loadButton_.setBounds(s(290), s(20), s(92), s(28));
    diagButton_.setBounds(s(388), s(20), s(84), s(28));
    instrumentName_.setBounds(getWidth() - s(360), s(12), s(344), s(24));
    status_.setBounds(getWidth() - s(360), s(36), s(344), s(18));

    // Articulation panel.
    articulationsHeader_.setBounds(s(26), s(82), s(170), s(16));
    int chipY = s(104);
    for (auto* chip : articulationChips_) {
        chip->setBounds(s(26), chipY, s(172), s(30));
        chipY += s(36);
    }

    // Performance panel.
    toneHeader_.setBounds(s(232), s(82), s(200), s(16));
    dynamics_->setBounds(s(238), s(104), s(140), s(158));
    expression_->setBounds(s(400), s(114), s(128), s(148));
    width_->setBounds(s(240), s(272), s(86), s(100));
    dnaAmount_->setBounds(s(340), s(272), s(86), s(100));
    dnaMode_.setBounds(s(436), s(300), s(100), s(26));
    master_->setBounds(s(452), s(340), s(76), s(88));

    // Stage + hall panel.
    stageHeader_.setBounds(s(572), s(82), s(160), s(16));
    stagePad_->setBounds(s(572), s(102), s(322), s(172));
    hallHeader_.setBounds(s(572), s(284), s(160), s(16));
    const int knobW = s(62), knobH = s(84);
    int hx = s(572);
    for (auto* k : {hallSize_.get(), hallDecay_.get(), hallDamping_.get(),
                    early_.get(), tail_.get()}) {
        k->setBounds(hx, s(304), knobW, knobH);
        hx += knobW + s(3);
    }

    // Footer strip.
    const int keyboardY = s(452);
    keyboard_->setBounds(s(14), keyboardY, getWidth() - s(28), s(96));
    keyboard_->setKeyWidth(float(keyboard_->getWidth()) / 56.5f);

    voicesLabel_.setBounds(s(14), s(556), s(90), s(20));
    meterArea_ = {s(110), s(560), s(150), s(14)};
    quality_.setBounds(getWidth() - s(250), s(556), s(92), s(24));
    limiter_.setBounds(getWidth() - s(150), s(556), s(90), s(24));
}

} // namespace sapporch
