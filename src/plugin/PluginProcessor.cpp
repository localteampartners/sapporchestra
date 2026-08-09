#include "PluginProcessor.h"

#include <sapp/sounds/DiagnosticInstrument.h>

#include "../core/SappLinkCCMap.h"
#include "PluginEditor.h"
#include "SappSettings.h"

namespace sapporch {

using namespace sapp::orchestra;
using sapp::sounds::MidiEvent;

namespace {
constexpr int kMaxArticulations = 16;
}

juce::AudioProcessorValueTreeState::ParameterLayout
SappOrchestraProcessor::makeLayout(std::vector<sapp::sfzlib::Entry>& outLibrary)
{
    using P = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Parameter IDs are compatibility contracts — never reuse or renumber.
    layout.add(std::make_unique<P>(juce::ParameterID{"dynamics", 1}, "Dynamics",
                                   Range{0.0f, 1.0f, 0.001f}, 0.7f));
    layout.add(std::make_unique<P>(juce::ParameterID{"expression", 1}, "Expression",
                                   Range{0.0f, 1.0f, 0.001f}, 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"stageX", 1}, "Stage Position",
                                   Range{-1.0f, 1.0f, 0.001f}, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"stageDepth", 1}, "Stage Depth",
                                   Range{0.0f, 1.0f, 0.001f}, 0.35f));
    layout.add(std::make_unique<P>(juce::ParameterID{"width", 1}, "Width",
                                   Range{0.0f, 2.0f, 0.001f}, 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"earlyLevel", 1}, "Early Reflections",
                                   Range{0.0f, 1.0f, 0.001f}, 0.35f));
    layout.add(std::make_unique<P>(juce::ParameterID{"tailLevel", 1}, "Hall Level",
                                   Range{0.0f, 1.0f, 0.001f}, 0.30f));
    layout.add(std::make_unique<P>(juce::ParameterID{"hallSize", 1}, "Hall Size",
                                   Range{0.2f, 1.5f, 0.001f}, 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"hallDecay", 1}, "Hall Decay",
                                   Range{0.3f, 12.0f, 0.01f, 0.5f}, 2.6f));
    layout.add(std::make_unique<P>(juce::ParameterID{"hallDamping", 1}, "Hall Damping",
                                   Range{0.0f, 1.0f, 0.001f}, 0.45f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"legato", 1}, "Legato", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"dnaMode", 1}, "Analog DNA Mode",
        juce::StringArray{"Clean", "Cohesive", "Vintage"}, 1));
    layout.add(std::make_unique<P>(juce::ParameterID{"dnaAmount", 1}, "Analog DNA Amount",
                                   Range{0.0f, 1.0f, 0.001f}, 0.18f));
    layout.add(std::make_unique<P>(juce::ParameterID{"masterGain", 1}, "Master Gain",
                                   Range{-24.0f, 12.0f, 0.1f}, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"limiter", 1}, "Safety Limiter", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"quality", 1}, "Quality",
        juce::StringArray{"Draft", "Normal"}, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"articulation", 1}, "Articulation", 0, kMaxArticulations - 1, 0));

    // APPENDED LAST (sapptune issue #20) so every pre-existing automation
    // index holds. Choice 0 keeps whatever is loaded; choice k loads library
    // entry k-1 (case-insensitive sort order — see SfzLibrary.h). The list is
    // fixed for this instance; a rescan shows up on the next instantiation.
    outLibrary = sapp::sfzlib::loadOrScan(
        sapp::sfzlib::resolveRoot(settings::samplesRoot().getFullPathName().toStdString()));
    juce::StringArray instrumentChoices;
    instrumentChoices.add("(keep current)");
    for (const auto& entry : outLibrary)
        instrumentChoices.add(juce::String::fromUTF8(entry.label.c_str()));
    if (outLibrary.empty())
        instrumentChoices.add("(no SFZ library found)");  // avoid a 1-step param
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"instrument", 1}, "Instrument", instrumentChoices, 0));
    return layout;
}

SappOrchestraProcessor::SappOrchestraProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SappOrchestra", makeLayout(sfzLibrary_))
{
    auto raw = [this](const char* id) { return apvts_.getRawParameterValue(id); };
    pDynamics_ = raw("dynamics");
    pExpression_ = raw("expression");
    pStageX_ = raw("stageX");
    pStageDepth_ = raw("stageDepth");
    pWidth_ = raw("width");
    pEarly_ = raw("earlyLevel");
    pTail_ = raw("tailLevel");
    pHallSize_ = raw("hallSize");
    pHallDecay_ = raw("hallDecay");
    pHallDamping_ = raw("hallDamping");
    pLegato_ = raw("legato");
    pDnaMode_ = raw("dnaMode");
    pDnaAmount_ = raw("dnaAmount");
    pMaster_ = raw("masterGain");
    pLimiter_ = raw("limiter");
    pQuality_ = raw("quality");
    pArticulation_ = raw("articulation");

    eventScratch_.reserve(512);

    const auto& table = sapplink::mappings();
    static_assert(std::tuple_size<decltype(ccSlews_)>::value == size_t(sapplink::kNumMappings),
                  "ccSlews_ must match the SappLink mapping table size");
    for (size_t i = 0; i < table.size(); ++i)
        ccSlews_[i].parameter = apvts_.getParameter(table[i].paramId);

    // Host-automatable SFZ selection: the callback may fire on the audio
    // thread, so it only stores an index; the timer applies it on the
    // message thread.
    apvts_.addParameterListener("instrument", this);
    startTimerHz(30);

    loadDiagnosticInstrument();
}

// --------------------------------------------- `instrument` choice param --

void SappOrchestraProcessor::parameterChanged(const juce::String& parameterId,
                                              float newValue)
{
    if (parameterId != juce::StringRef("instrument") || applyingInstrumentChoice_)
        return;
    pendingInstrumentChoice_.store(int(newValue + 0.5f));
}

void SappOrchestraProcessor::timerCallback()
{
    // MIDI program select first, then an explicit parameter move: when both
    // land in the same tick the parameter (the deliberate host move) wins.
    const int programSelect = pendingProgramSelect_.exchange(-1);
    if (programSelect >= 0)
        applyProgramSelect(programSelect >> 16, programSelect & 0xffff);
    const int choice = pendingInstrumentChoice_.exchange(-1);
    if (choice >= 0)
        applyInstrumentChoice(choice);
}

void SappOrchestraProcessor::applyInstrumentChoice(int choiceIndex)
{
    if (choiceIndex <= 0 || choiceIndex > int(sfzLibrary_.size()))
        return;  // "(keep current)" / placeholder / out of range
    const auto& entry = sfzLibrary_[size_t(choiceIndex - 1)];
    const juce::File file(juce::String::fromUTF8(entry.path.c_str()));
    if (file.getFullPathName() == currentInstrumentPath())
        return;  // already loaded in the selected slot
    if (!file.existsAsFile()) {
        // Graceful miss: the library changed since the index was written.
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Missing: " + file.getFullPathName();
        if (onInstrumentChanged) onInstrumentChanged();
        return;
    }
    loadSfzInstrument(file);
}

void SappOrchestraProcessor::applyProgramSelect(int slot, int entryIndex)
{
    if (slot < 0 || slot >= 16 || entryIndex < 0 || entryIndex >= int(sfzLibrary_.size()))
        return;
    const auto& entry = sfzLibrary_[size_t(entryIndex)];
    const juce::File file(juce::String::fromUTF8(entry.path.c_str()));
    if (file.getFullPathName() == slotPaths_[size_t(slot)])
        return;
    if (!file.existsAsFile()) {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Missing: " + file.getFullPathName();
        if (onInstrumentChanged) onInstrumentChanged();
        return;
    }
    loadSfzInstrumentIntoSlot(file, slot);
}

void SappOrchestraProcessor::syncInstrumentParameter(const juce::String& path)
{
    auto* parameter = apvts_.getParameter("instrument");
    if (parameter == nullptr) return;
    int choice = 0;  // "" / unknown path -> "(keep current)"
    const auto pathStd = path.toStdString();
    for (size_t i = 0; i < sfzLibrary_.size(); ++i)
        if (sfzLibrary_[i].path == pathStd) { choice = int(i) + 1; break; }
    applyingInstrumentChoice_ = true;
    parameter->setValueNotifyingHost(
        parameter->convertTo0to1(float(choice)));
    applyingInstrumentChoice_ = false;
}

bool SappOrchestraProcessor::rescanSfzLibrary() const
{
    const auto root = sapp::sfzlib::resolveRoot(
        settings::samplesRoot().getFullPathName().toStdString());
    return sapp::sfzlib::writeIndex(root, sapp::sfzlib::scan(root));
}

void SappOrchestraProcessor::handleSappLinkCc(int ccNumber, int ccValue)
{
    // CC 16/17/18 (stage) are per-channel and handled inside the engine.
    if (ccNumber == 16 || ccNumber == 17 || ccNumber == 18) return;
    const auto* mapping = sapplink::findMapping(ccNumber);
    if (mapping == nullptr)
        return;
    const auto index = size_t(mapping - sapplink::mappings().data());
    auto& slew = ccSlews_[index];
    if (slew.parameter == nullptr)
        return;
    slew.target = slew.parameter->convertTo0to1(sapplink::ccToEngineering(*mapping, ccValue));
    if (!slew.active)
        slew.current = slew.parameter->getValue();
    slew.active = true;
}

void SappOrchestraProcessor::advanceCcSlews(int numSamples)
{
    // ~15 ms approach per step, applied through the same normalized-value
    // path host automation uses — never straight into the DSP.
    const float coefficient =
        1.0f - std::exp(-float(numSamples) / (0.015f * float(getSampleRate() > 0 ? getSampleRate() : 48000.0)));
    for (auto& slew : ccSlews_) {
        if (!slew.active || slew.parameter == nullptr)
            continue;
        slew.current += (slew.target - slew.current) * coefficient;
        if (std::abs(slew.target - slew.current) < 1.0e-4f) {
            slew.current = slew.target;
            slew.active = false;
        }
        slew.parameter->setValueNotifyingHost(slew.current);
    }
}

SappOrchestraProcessor::~SappOrchestraProcessor()
{
    stopTimer();
    apvts_.removeParameterListener("instrument", this);
}

void SappOrchestraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, juce::jmax(64, samplesPerBlock));
    pushParamsToEngine();
}

void SappOrchestraProcessor::releaseResources() {}

bool SappOrchestraProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SappOrchestraProcessor::pushParamsToEngine()
{
    OrchestraParams p;
    p.dynamics = pDynamics_->load();
    p.expression = pExpression_->load();
    p.stageX = pStageX_->load();
    p.stageDepth = pStageDepth_->load();
    p.width = pWidth_->load();

    // Stage params edit the SELECTED slot; write through only on change so
    // per-channel CC16/17/18 (handled inside the engine) are not clobbered.
    if (p.stageX != lastStageX_ || p.stageDepth != lastStageDepth_ || p.width != lastWidth_) {
        lastStageX_ = p.stageX;
        lastStageDepth_ = p.stageDepth;
        lastWidth_ = p.width;
        engine_.setSlotStage(selectedSlot_, p.stageX, p.stageDepth, p.width);
    }
    p.earlyLevel = pEarly_->load();
    p.tailLevel = pTail_->load();
    p.hallSize = pHallSize_->load();
    p.hallDecay = pHallDecay_->load();
    p.hallDamping = pHallDamping_->load();
    p.legato = pLegato_->load();
    p.dnaMode = int(pDnaMode_->load());
    p.dnaAmount = pDnaAmount_->load();
    p.masterGainDb = pMaster_->load();
    p.limiter = pLimiter_->load() > 0.5f;
    p.quality = int(pQuality_->load());
    engine_.setParams(p);
}

void SappOrchestraProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    advanceCcSlews(buffer.getNumSamples());
    pushParamsToEngine();

    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);

    eventScratch_.clear();

    // Knob→CC bridging: moving Dynamics/Expression behaves like riding CC1/11.
    const float dynParam = pDynamics_->load();
    if (lastDynParam_ >= 0.0f && std::abs(dynParam - lastDynParam_) > 0.004f) {
        MidiEvent e;
        e.type = MidiEvent::Type::Controller;
        e.frame = 0;
        e.note = 1;
        e.value = uint8_t(juce::jlimit(0, 127, int(dynParam * 127.0f + 0.5f)));
        eventScratch_.push_back(e);
    }
    lastDynParam_ = dynParam;
    const float exprParam = pExpression_->load();
    if (lastExprParam_ >= 0.0f && std::abs(exprParam - lastExprParam_) > 0.004f) {
        MidiEvent e;
        e.type = MidiEvent::Type::Controller;
        e.frame = 0;
        e.note = 11;
        e.value = uint8_t(juce::jlimit(0, 127, int(exprParam * 127.0f + 0.5f)));
        eventScratch_.push_back(e);
    }
    lastExprParam_ = exprParam;

    // Automatable articulation parameter.
    const int artParam = int(pArticulation_->load());
    if (artParam != lastArticulationParam_) {
        lastArticulationParam_ = artParam;
        engine_.selectArticulation(artParam, selectedSlot_);
    }

    for (const auto metadata : midi) {
        const auto msg = metadata.getMessage();
        MidiEvent e;
        e.frame = uint32_t(juce::jmax(0, metadata.samplePosition));
        e.channel = uint8_t(juce::jlimit(1, 16, msg.getChannel() > 0 ? msg.getChannel() : 1) - 1);
        if (msg.isNoteOn()) {
            e.type = MidiEvent::Type::NoteOn;
            e.note = uint8_t(msg.getNoteNumber());
            e.value = uint8_t(msg.getVelocity());
        } else if (msg.isNoteOff()) {
            e.type = MidiEvent::Type::NoteOff;
            e.note = uint8_t(msg.getNoteNumber());
        } else if (msg.isController()) {
            e.type = MidiEvent::Type::Controller;
            e.note = uint8_t(msg.getControllerNumber());
            e.value = uint8_t(msg.getControllerValue());
            // Bank select (per channel) arms the next program change:
            // entry = ((CC0 << 7) | CC32) * 128 + program (sapptune #20).
            if (msg.getControllerNumber() == 0)
                bankMsb_[e.channel] = uint8_t(msg.getControllerValue());
            else if (msg.getControllerNumber() == 32)
                bankLsb_[e.channel] = uint8_t(msg.getControllerValue());
            // SappLink CC-in (any channel): mapped CCs also steer parameters.
            // The event still reaches the engine below (SFZ CC conditions,
            // native CC1/CC11/CC64 behavior stay untouched).
            handleSappLinkCc(msg.getControllerNumber(), msg.getControllerValue());
        } else if (msg.isProgramChange()) {
            // Program change selects an SFZ library entry for THIS channel's
            // slot; the load itself happens on the message thread (timer).
            const int bank = (int(bankMsb_[e.channel]) << 7) | int(bankLsb_[e.channel]);
            const int entry = bank * 128 + msg.getProgramChangeNumber();
            if (entry < int(sfzLibrary_.size()))
                pendingProgramSelect_.store((int(e.channel) << 16) | entry);
            continue;  // consumed; the engine has no program-change concept
        } else if (msg.isPitchWheel()) {
            e.type = MidiEvent::Type::PitchBend;
            e.bend14 = int16_t(msg.getPitchWheelValue() - 8192);
        } else if (msg.isAllNotesOff()) {
            e.type = MidiEvent::Type::AllNotesOff;
        } else if (msg.isAllSoundOff()) {
            e.type = MidiEvent::Type::AllSoundOff;
        } else {
            continue;
        }
        eventScratch_.push_back(e);
    }
    std::stable_sort(eventScratch_.begin(), eventScratch_.end(),
                     [](const MidiEvent& a, const MidiEvent& b) { return a.frame < b.frame; });

    buffer.clear();
    if (buffer.getNumChannels() >= 2) {
        engine_.process(eventScratch_.data(), int(eventScratch_.size()),
                        buffer.getWritePointer(0), buffer.getWritePointer(1),
                        buffer.getNumSamples());
    }
    midi.clear();
}

// ------------------------------------------------------------- instruments --

void SappOrchestraProcessor::loadDiagnosticInstrument()
{
    const uint64_t generation = ++loadGeneration_;
    const int slot = selectedSlot_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Generating diagnostic orchestra...";
    }
    std::thread([this, generation, slot] {
        auto inst = sapp::sounds::makeDiagnosticInstrument();
        sapp::sounds::LoadResult result;
        result.instrument = inst;
        result.ok = true;
        juce::MessageManager::callAsync([this, result = std::move(result), generation, slot]() mutable {
            finishLoad(std::move(result), {}, generation, slot);
        });
    }).detach();
}

void SappOrchestraProcessor::selectSlot(int slot)
{
    slot = juce::jlimit(0, sapp::orchestra::OrchestraEngine::kNumSlots - 1, slot);
    if (slot == selectedSlot_) return;
    selectedSlot_ = slot;

    // Reflect the slot's stage into the (selected-slot-scoped) APVTS params.
    float x = 0, depth = 0, width = 1;
    engine_.getSlotStage(slot, x, depth, width);
    lastStageX_ = x;
    lastStageDepth_ = depth;
    lastWidth_ = width;
    auto reflect = [this](const char* id, float value) {
        if (auto* param = apvts_.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    reflect("stageX", x);
    reflect("stageDepth", depth);
    reflect("width", width);
    // The `instrument` parameter is selected-slot-scoped, like the stage
    // params: reflect this slot's loaded SFZ (guarded, no reload).
    syncInstrumentParameter(slotPaths_[size_t(slot)]);
    lastArticulationParam_ = -1;  // re-apply on next block
    if (onInstrumentChanged) onInstrumentChanged();
}

void SappOrchestraProcessor::setSlotMix(int slot, float gainDb, bool mute, bool solo)
{
    engine_.setSlotMix(slot, gainDb, mute, solo);
}

void SappOrchestraProcessor::getSlotMix(int slot, float& gainDb, bool& mute, bool& solo) const
{
    const_cast<sapp::orchestra::OrchestraEngine&>(engine_).getSlotMix(slot, gainDb, mute, solo);
}

// --- Full Orchestra factory preset (Sonatina, 16 channels) -----------------

namespace {
struct PresetSlot {
    const char* library;    // library folder name searched under samplesRoot
    const char* fileName;   // filename or wildcard; "" leaves the slot empty
    float x, depth, gainDb;
};
struct OrchestraPresetDef {
    const char* name;       // shown on the panel button
    PresetSlot slots[16];
};
// Classic seating, audience view (negative x = left).
#define SON "Sonatina Symphonic Orchestra"
#define VPO3 "Virtual-Playing-Orchestra3"
#define VSCO "vsco2-ce"
const OrchestraPresetDef kOrchestraPresets[] = {
    {"ORCHESTRA: SONATINA",
     {{SON, "1st Violins KS.sfz", -0.65f, 0.25f, 0.0f},
      {SON, "2nd Violins KS.sfz", -0.30f, 0.30f, 0.0f},
      {SON, "Violas KS.sfz", 0.25f, 0.30f, 0.0f},
      {SON, "Celli KS.sfz", 0.55f, 0.35f, 0.0f},
      {SON, "Basses KS.sfz", 0.75f, 0.50f, 0.0f},
      {SON, "Flutes KS.sfz", -0.10f, 0.50f, -2.0f},
      {SON, "Oboes KS.sfz", 0.15f, 0.50f, -2.0f},
      {SON, "Clarinets KS.sfz", 0.15f, 0.60f, -2.0f},
      {SON, "Bassoons KS.sfz", -0.10f, 0.60f, -2.0f},
      {SON, "Horns KS.sfz", -0.45f, 0.65f, -1.0f},
      {SON, "Trumpets KS.sfz", 0.30f, 0.70f, -3.0f},
      {SON, "Trombones KS.sfz", 0.45f, 0.70f, -3.0f},
      {SON, "Tuba KS.sfz", 0.60f, 0.72f, -3.0f},
      {SON, "Timpani.sfz", 0.10f, 0.85f, -2.0f},
      {SON, "Concert Harp.sfz", -0.70f, 0.60f, -2.0f},
      {SON, "Mixed Chorus.sfz", 0.00f, 0.90f, -4.0f}}},
    {"ORCHESTRA: VPO",
     {{VPO3, "1st-violin-SEC-KS-C2.sfz", -0.65f, 0.25f, 0.0f},
      {VPO3, "2nd-violin-SEC-KS-C2.sfz", -0.30f, 0.30f, 0.0f},
      {VPO3, "viola-SEC-KS-C2.sfz", 0.25f, 0.30f, 0.0f},
      {VPO3, "cello-SEC-KS-C6.sfz", 0.55f, 0.35f, 0.0f},
      {VPO3, "bass-SEC-KS-C6.sfz", 0.75f, 0.50f, 0.0f},
      {VPO3, "flute-SEC-KS-C2.sfz", -0.10f, 0.50f, -2.0f},
      {VPO3, "oboe-SEC-KS-C2.sfz", 0.15f, 0.50f, -2.0f},
      {VPO3, "clarinet-SEC-KS-C2.sfz", 0.15f, 0.60f, -2.0f},
      {VPO3, "bassoon-SEC-KS-C6.sfz", -0.10f, 0.60f, -2.0f},
      {VPO3, "french-horn-SEC-KS-C6-DXF.sfz", -0.45f, 0.65f, -1.0f},
      {VPO3, "trumpet-SEC-KS-C2-DXF.sfz", 0.30f, 0.70f, -3.0f},
      {VPO3, "trombone-SEC-KS-C6-DXF.sfz", 0.45f, 0.70f, -3.0f},
      {VPO3, "tuba-SOLO-KS-C6.sfz", 0.60f, 0.72f, -3.0f},
      {VPO3, "timpani-KS-C6.sfz", 0.10f, 0.85f, -2.0f},
      {VPO3, "harp-KS-C0.sfz", -0.70f, 0.60f, -2.0f},
      {VPO3, "choir-MIXED-normal-mod-wheel.sfz", 0.00f, 0.90f, -4.0f}}},
    {"ORCHESTRA: VSCO2",
     {{VSCO, "ViolinEns-KS.sfz", -0.65f, 0.25f, 0.0f},
      {VSCO, "SViolin-KS.sfz", -0.30f, 0.30f, -2.0f},
      {VSCO, "ViolaEns-KS.sfz", 0.25f, 0.30f, 0.0f},
      {VSCO, "CelloEns-KS.sfz", 0.55f, 0.35f, 0.0f},
      {VSCO, "Contrabass-KS.sfz", 0.75f, 0.50f, 0.0f},
      {VSCO, "Flute-KS.sfz", -0.10f, 0.50f, -2.0f},
      {VSCO, "OboeSusVib.sfz", 0.15f, 0.50f, -2.0f},
      {VSCO, "Clarinet-KS.sfz", 0.15f, 0.60f, -2.0f},
      {VSCO, "BassoonSus.sfz", -0.10f, 0.60f, -2.0f},
      {VSCO, "FHornSus.sfz", -0.45f, 0.65f, -1.0f},
      {VSCO, "TrumpetSusVib.sfz", 0.30f, 0.70f, -3.0f},
      {VSCO, "TromboneSus.sfz", 0.45f, 0.70f, -3.0f},
      {VSCO, "Tuba-KS.sfz", 0.60f, 0.72f, -3.0f},
      {VSCO, "Timpani.sfz", 0.10f, 0.85f, -2.0f},
      {VSCO, "Harp.sfz", -0.70f, 0.60f, -2.0f},
      {VSCO, "Glockenspiel.sfz", -0.30f, 0.85f, -4.0f}}},
    {"DRUMS + PERC",
     {{"avl-drumkits", "Black_Pearl_5pc.sfz", 0.00f, 0.30f, 0.0f},
      {"avl-drumkits", "Black_Pearl_4pc.sfz", -0.20f, 0.30f, 0.0f},
      {"avl-drumkits", "Red_Zeppelin_5pc.sfz", 0.20f, 0.30f, 0.0f},
      {"avl-drumkits", "Red_Zeppelin_4pc.sfz", 0.40f, 0.35f, 0.0f},
      {"big-rusty-drums", "*usty*.sfz", -0.40f, 0.35f, 0.0f},
      {"sm-drums", "*.sfz", -0.60f, 0.40f, 0.0f},
      {VSCO, "GM-StylePerc.sfz", 0.00f, 0.50f, -1.0f},
      {VSCO, "Timpani.sfz", 0.10f, 0.85f, -2.0f},
      {VSCO, "TimpaniRolls.sfz", 0.15f, 0.85f, -2.0f},
      {SON, "Timpani.sfz", 0.05f, 0.90f, -2.0f},
      {VSCO, "Glockenspiel.sfz", -0.35f, 0.75f, -4.0f},
      {VSCO, "Xylophone.sfz", 0.35f, 0.75f, -4.0f},
      {VSCO, "Marimba.sfz", -0.50f, 0.70f, -3.0f},
      {VSCO, "TubularBells.sfz", 0.50f, 0.80f, -4.0f},
      {VPO3, "timpani-hit-n-roll.sfz", 0.00f, 0.92f, -2.0f},
      {SON, "All Unpitched Percussion.sfz", -0.15f, 0.92f, -3.0f}}},
    {"PIANOS + KEYS",
     {{"salamander", "SalamanderGrandPiano-V3*.sfz", -0.15f, 0.25f, 0.0f},
      {"upright-piano", "UprightPianoKW*.sfz", -0.45f, 0.30f, -1.0f},
      {"fm-piano1", "FM-Piano1*.sfz", 0.35f, 0.30f, -2.0f},
      {"old-piano-fb", "*.sfz", 0.60f, 0.35f, -2.0f},
      {SON, "Grand Piano.sfz", 0.00f, 0.45f, -1.0f},
      {VSCO, "UprightPiano.sfz", -0.60f, 0.40f, -2.0f},
      {SON, "Harpsichord*.sfz", 0.55f, 0.45f, -3.0f},
      {VPO3, "celesta.sfz", 0.30f, 0.55f, -4.0f},
      {SON, "Celeste.sfz", -0.30f, 0.55f, -4.0f},
      {VSCO, "OrganQuiet.sfz", 0.00f, 0.80f, -3.0f},
      {VSCO, "OrganLoud.sfz", 0.00f, 0.85f, -6.0f},
      {SON, "Organ*.sfz", -0.20f, 0.85f, -6.0f},
      {SON, "Concert Harp.sfz", -0.70f, 0.50f, -2.0f},
      {VSCO, "Harp.sfz", 0.70f, 0.50f, -2.0f},
      {VSCO, "Glockenspiel.sfz", -0.40f, 0.70f, -5.0f},
      {VSCO, "Marimba.sfz", 0.40f, 0.70f, -4.0f}}},
    {"CHOIR + VOICES",
     {{SON, "Mixed Chorus.sfz", 0.00f, 0.50f, 0.0f},
      {SON, "Large Chrous.sfz", 0.00f, 0.70f, -1.0f},
      {VPO3, "choir-MIXED-normal-mod-wheel.sfz", 0.00f, 0.60f, -1.0f},
      {VPO3, "choir-FEMALE-sustain.sfz", -0.35f, 0.55f, -1.0f},
      {VPO3, "choir-MALE-sustain.sfz", 0.35f, 0.55f, -1.0f},
      {"freepats-synth-choir", "SynthPadChoir*.sfz", 0.00f, 0.40f, -3.0f},
      {"legato-vocal", "09-complete_original_legato_a.sfz", 0.00f, 0.20f, -2.0f},
      {"legato-vocal", "01-no_legato_polyphonic_only_no_vibrato_a.sfz", -0.20f, 0.30f, -3.0f},
      {VPO3, "choir-FEMALE-PERF.sfz", -0.50f, 0.65f, -2.0f},
      {VPO3, "choir-MALE-PERF.sfz", 0.50f, 0.65f, -2.0f},
      {SON, "Organ*.sfz", 0.00f, 0.90f, -8.0f},
      {SON, "Concert Harp.sfz", -0.70f, 0.50f, -3.0f},
      {"", "", 0.0f, 0.35f, 0.0f},
      {"", "", 0.0f, 0.35f, 0.0f},
      {"", "", 0.0f, 0.35f, 0.0f},
      {"", "", 0.0f, 0.35f, 0.0f}}},
};
#undef SON
#undef VPO3
#undef VSCO
constexpr int kNumOrchestraPresets = 6;
} // namespace

int SappOrchestraProcessor::orchestraPresetCount() const { return kNumOrchestraPresets; }

juce::String SappOrchestraProcessor::orchestraPresetName(int preset) const
{
    if (preset < 0 || preset >= kNumOrchestraPresets) return {};
    return kOrchestraPresets[preset].name;
}

juce::File SappOrchestraProcessor::findLibraryDir(const juce::String& dirName) const
{
    if (dirName.isEmpty()) return {};
    // Cached: panel timers poll availability, and the recursive scan of a
    // large samples tree is not free.
    const auto now = juce::Time::getMillisecondCounter();
    auto it = libraryRootCache_.find(dirName);
    if (it != libraryRootCache_.end() && now - it->second.second < 10000)
        return it->second.first;
    const auto root = settings::samplesRoot();
    juce::File found;
    if (root.getChildFile(dirName).isDirectory())
        found = root.getChildFile(dirName);
    else
        for (const auto& dir : root.findChildFiles(juce::File::findDirectories, true))
            if (dir.getFileName() == dirName) { found = dir; break; }
    libraryRootCache_[dirName] = {found, now};
    return found;
}

bool SappOrchestraProcessor::orchestraPresetAvailable(int preset) const
{
    if (preset < 0 || preset >= kNumOrchestraPresets) return false;
    for (const auto& slot : kOrchestraPresets[preset].slots)
        if (slot.fileName[0] != 0 && findLibraryDir(slot.library).isDirectory())
            return true;
    return false;
}

bool SappOrchestraProcessor::loadOrchestraPreset(int preset)
{
    if (!orchestraPresetAvailable(preset)) return false;
    activePreset_ = preset;
    const uint64_t generation = ++loadGeneration_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Setting up the orchestra (1/16)...";
    }
    // Seats + balance apply immediately; instruments stream in one by one.
    const auto& def = kOrchestraPresets[preset];
    for (int i = 0; i < 16; ++i) {
        engine_.setSlotStage(i, def.slots[i].x, def.slots[i].depth, 1.0f);
        engine_.setSlotMix(i, def.slots[i].gainDb, false, false);
    }
    selectedSlot_ = -1;   // force the slot-0 reselect to reflect its new seat
    selectSlot(0);
    loadOrchestraPresetStep(0, generation);
    return true;
}

void SappOrchestraProcessor::loadOrchestraPresetStep(size_t step, uint64_t generation)
{
    if (generation != loadGeneration_.load()) return;  // superseded
    if (step >= 16) {
        loading_ = false;
        {
            const juce::ScopedLock sl(loadLock_);
            loadStatus_ = "Full orchestra ready - 16 channels";
        }
        if (onInstrumentChanged) onInstrumentChanged();
        return;
    }
    const auto& slotDef = kOrchestraPresets[activePreset_].slots[step];
    const juce::String fileName(slotDef.fileName);
    juce::File file;
    if (fileName.isNotEmpty()) {
        const auto libraryRoot = findLibraryDir(slotDef.library);
        if (libraryRoot.isDirectory())
            for (const auto& candidate :
                 libraryRoot.findChildFiles(juce::File::findFiles, true, fileName)) {
                const auto p = candidate.getFullPathName();
                if (!p.contains("includes") && !p.contains("modules")) { file = candidate; break; }
            }
    }

    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Loading " + fileName.upToLastOccurrenceOf(".sfz", false, true) +
                      " (" + juce::String(int(step) + 1) + "/16)...";
    }
    if (onInstrumentChanged) onInstrumentChanged();

    if (!file.existsAsFile()) {
        loadOrchestraPresetStep(step + 1, generation);
        return;
    }
    const juce::String path = file.getFullPathName();
    const int slot = int(step);
    std::thread([this, path, generation, slot, step] {
        sapp::sounds::InstrumentLoader loader;
        auto result = loader.loadSfz(path.toStdString());
        juce::MessageManager::callAsync(
            [this, result = std::move(result), path, generation, slot, step]() mutable {
                if (generation != loadGeneration_.load()) return;
                if (result.ok && result.instrument != nullptr) {
                    const juce::ScopedLock sl(loadLock_);
                    engine_.setInstrument(result.instrument, slot);
                    engine_.collectRetired();
                    slotPaths_[size_t(slot)] = path;
                    slotNames_[size_t(slot)] =
                        juce::String(result.instrument->definition.name);
                }
                if (result.ok && slot == selectedSlot_)
                    syncInstrumentParameter(slotPaths_[size_t(slot)]);
                if (onInstrumentChanged) onInstrumentChanged();
                loadOrchestraPresetStep(step + 1, generation);
            });
    }).detach();
}

juce::String SappOrchestraProcessor::slotName(int slot) const
{
    if (slot < 0 || slot >= 16) return {};
    const juce::ScopedLock sl(loadLock_);
    return slotNames_[size_t(slot)];
}

void SappOrchestraProcessor::loadSfzInstrument(const juce::File& sfzFile)
{
    loadSfzInstrumentIntoSlot(sfzFile, selectedSlot_);
}

void SappOrchestraProcessor::loadSfzInstrumentIntoSlot(const juce::File& sfzFile, int slot)
{
    const uint64_t generation = ++loadGeneration_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Loading " + sfzFile.getFileName() + "...";
    }
    const juce::String path = sfzFile.getFullPathName();
    std::thread([this, path, generation, slot] {
        sapp::sounds::InstrumentLoader loader;
        auto result = loader.loadSfz(path.toStdString());
        juce::MessageManager::callAsync([this, result = std::move(result), path, generation, slot]() mutable {
            finishLoad(std::move(result), path, generation, slot);
        });
    }).detach();
}

void SappOrchestraProcessor::finishLoad(sapp::sounds::LoadResult result,
                                        const juce::String& path, uint64_t generation,
                                        int slot)
{
    if (generation != loadGeneration_.load()) loading_ = false;  // superseded but note it
    loading_ = false;

    const juce::ScopedLock sl(loadLock_);
    if (!result.ok || result.instrument == nullptr) {
        loadStatus_ = "Load failed";
        for (const auto& d : result.diagnostics)
            if (d.severity == sapp::sounds::Severity::Error) {
                loadStatus_ = "Load failed: " + juce::String(d.message);
                break;
            }
    } else {
        engine_.setInstrument(result.instrument, slot);
        engine_.collectRetired();
        slotPaths_[size_t(slot)] = path;
        slotNames_[size_t(slot)] = juce::String(result.instrument->definition.name);
        loadStatus_ = result.missingSamples.empty()
                          ? "Ready"
                          : juce::String(result.missingSamples.size()) + " samples missing";
        lastArticulationParam_ = -1;  // re-apply articulation on next block
    }
    // Reflect the selected slot's instrument in the `instrument` parameter
    // (guarded — this must not schedule another load).
    if (slot == selectedSlot_)
        syncInstrumentParameter(slotPaths_[size_t(slot)]);
    if (onInstrumentChanged) onInstrumentChanged();
}

juce::String SappOrchestraProcessor::currentInstrumentName() const
{
    const juce::ScopedLock sl(loadLock_);
    const auto name = slotNames_[size_t(selectedSlot_)];
    return name.isEmpty() ? "(empty)" : name;
}

juce::String SappOrchestraProcessor::loadStatus() const
{
    const juce::ScopedLock sl(loadLock_);
    return loadStatus_;
}

juce::StringArray SappOrchestraProcessor::articulationNames() const
{
    juce::StringArray names;
    if (auto inst = engine_.currentInstrument(selectedSlot_))
        for (const auto& a : inst->definition.articulations)
            names.add(juce::String(a.name));
    return names;
}

int SappOrchestraProcessor::currentArticulation() const
{
    return int(pArticulation_->load());
}

void SappOrchestraProcessor::selectArticulation(int index)
{
    if (auto* param = apvts_.getParameter("articulation")) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(
            param->convertTo0to1(float(juce::jlimit(0, kMaxArticulations - 1, index))));
        param->endChangeGesture();
    }
}

// -------------------------------------------------------------------- state --

void SappOrchestraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty("sfzPath", slotPaths_[0], nullptr);  // legacy readers
    state.setProperty("stateVersion", 2, nullptr);
    state.setProperty("selectedSlot", selectedSlot_, nullptr);
    juce::ValueTree slots("slots");
    for (int i = 0; i < 16; ++i) {
        float x = 0, depth = 0, width = 1;
        engine_.getSlotStage(i, x, depth, width);
        juce::ValueTree slot("slot");
        slot.setProperty("index", i, nullptr);
        slot.setProperty("path", slotPaths_[size_t(i)], nullptr);
        slot.setProperty("stageX", x, nullptr);
        slot.setProperty("stageDepth", depth, nullptr);
        slot.setProperty("width", width, nullptr);
        float gainDb = 0;
        bool mute = false, solo = false;
        engine_.getSlotMix(i, gainDb, mute, solo);
        slot.setProperty("gainDb", gainDb, nullptr);
        slot.setProperty("mute", mute, nullptr);
        slot.setProperty("solo", solo, nullptr);
        slots.appendChild(slot, nullptr);
    }
    state.appendChild(slots, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SappOrchestraProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid()) return;
        // The chosen SFZ persists BY PATH (indices shift when the library
        // changes): suppress the `instrument` choice value coming back with
        // the tree so it cannot race the path-based loads below. finishLoad
        // re-syncs the parameter once the real instrument is in.
        applyingInstrumentChoice_ = true;
        apvts_.replaceState(state);
        applyingInstrumentChoice_ = false;
        pendingInstrumentChoice_.store(-1);
        const auto slots = state.getChildWithName("slots");
        bool loadedAny = false;
        if (slots.isValid()) {
            for (int i = 0; i < slots.getNumChildren(); ++i) {
                const auto slot = slots.getChild(i);
                const int index = int(slot.getProperty("index", i));
                if (index < 0 || index >= 16) continue;
                engine_.setSlotStage(index, float(slot.getProperty("stageX", 0.0f)),
                                     float(slot.getProperty("stageDepth", 0.35f)),
                                     float(slot.getProperty("width", 1.0f)));
                engine_.setSlotMix(index, float(slot.getProperty("gainDb", 0.0f)),
                                   bool(slot.getProperty("mute", false)),
                                   bool(slot.getProperty("solo", false)));
                const juce::String path = slot.getProperty("path", "").toString();
                if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
                    loadSfzInstrumentIntoSlot(juce::File(path), index);
                    loadedAny = true;
                }
            }
            selectSlot(int(state.getProperty("selectedSlot", 0)));
        } else {
            const juce::String path = state.getProperty("sfzPath", "").toString();
            if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
                loadSfzInstrument(juce::File(path));
                loadedAny = true;
            }
        }
        if (!loadedAny)
            loadDiagnosticInstrument();
    }
}

juce::AudioProcessorEditor* SappOrchestraProcessor::createEditor()
{
    return new SappOrchestraEditor(*this);
}

} // namespace sapporch

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sapporch::SappOrchestraProcessor();
}
