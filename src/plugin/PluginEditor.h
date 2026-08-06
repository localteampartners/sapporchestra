#pragma once
// SappOrchestra editor — "concert hall at night".
// Deep warm charcoal, brass/gold accents, vector-drawn controls.

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace sapporch {

// ------------------------------------------------------------------ palette --
namespace palette {
const juce::Colour background{0xff191512};   // deep warm charcoal
const juce::Colour panel{0xff211c17};
const juce::Colour panelEdge{0xff2e2720};
const juce::Colour gold{0xffc9a15c};         // brass
const juce::Colour goldBright{0xffe6c684};
const juce::Colour ivory{0xffe9e1d2};
const juce::Colour dim{0xff8d8273};
const juce::Colour burgundy{0xff77393b};
const juce::Colour shadow{0xff0e0c0a};
} // namespace palette

// ------------------------------------------------------------ look and feel --
class OrchestraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OrchestraLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// ------------------------------------------------------------- labeled knob --
class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
         const juce::String& title, bool big = false);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label_;
    bool big_;
};

// --------------------------------------------------------------- stage pad ---
// XY control: horizontal = stageX (-1..1), vertical = stageDepth (0 front .. 1 back).
class StagePad : public juce::Component, private juce::Timer
{
public:
    explicit StagePad(juce::AudioProcessorValueTreeState& state);
    ~StagePad() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void applyPoint(juce::Point<float> p);
    juce::AudioProcessorValueTreeState& state_;
    juce::RangedAudioParameter* xParam_;
    juce::RangedAudioParameter* yParam_;
    float glow_ = 0.0f;
};

// ------------------------------------------------------- keyswitch keyboard --
class OrchestraKeyboard : public juce::MidiKeyboardComponent
{
public:
    OrchestraKeyboard(SappOrchestraProcessor& processor, juce::MidiKeyboardState& state);

protected:
    void drawWhiteNote(int midiNoteNumber, juce::Graphics&, juce::Rectangle<float>,
                       bool isDown, bool isOver, juce::Colour lineColour,
                       juce::Colour textColour) override;
    void drawBlackNote(int midiNoteNumber, juce::Graphics&, juce::Rectangle<float>,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    bool keyswitchInfo(int note, bool& isActive) const;
    SappOrchestraProcessor& processor_;
};

// ------------------------------------------------------------------- editor --
class SappOrchestraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SappOrchestraEditor(SappOrchestraProcessor&);
    ~SappOrchestraEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildArticulationChips();
    void chooseSfz();

    SappOrchestraProcessor& processor_;
    OrchestraLookAndFeel lookAndFeel_;

    juce::Label title_, subtitle_, instrumentName_, status_;
    juce::TextButton loadButton_{"LOAD SFZ"};
    juce::TextButton diagButton_{"BUILT-IN"};

    juce::Label articulationsHeader_, stageHeader_, hallHeader_, toneHeader_;
    juce::OwnedArray<juce::TextButton> articulationChips_;

    std::unique_ptr<Knob> dynamics_, expression_, width_, dnaAmount_;
    std::unique_ptr<Knob> hallSize_, hallDecay_, hallDamping_, early_, tail_;
    std::unique_ptr<Knob> master_;
    juce::ComboBox dnaMode_, quality_;
    juce::ToggleButton limiter_{"limiter"};
    std::unique_ptr<StagePad> stagePad_;
    std::unique_ptr<OrchestraKeyboard> keyboard_;

    juce::Label voicesLabel_;
    float meterL_ = 0.0f, meterR_ = 0.0f;
    juce::Rectangle<int> meterArea_;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;
    std::unique_ptr<ComboAttachment> dnaModeAttachment_, qualityAttachment_;
    std::unique_ptr<ButtonAttachment> limiterAttachment_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappOrchestraEditor)
};

} // namespace sapporch
