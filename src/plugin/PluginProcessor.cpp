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

juce::AudioProcessorValueTreeState::ParameterLayout SappOrchestraProcessor::makeLayout()
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
    return layout;
}

SappOrchestraProcessor::SappOrchestraProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SappOrchestra", makeLayout())
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

    loadDiagnosticInstrument();
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

SappOrchestraProcessor::~SappOrchestraProcessor() = default;

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
            // SappLink CC-in (any channel): mapped CCs also steer parameters.
            // The event still reaches the engine below (SFZ CC conditions,
            // native CC1/CC11/CC64 behavior stay untouched).
            handleSappLinkCc(msg.getControllerNumber(), msg.getControllerValue());
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
    const char* fileName;   // located by name anywhere under the Sonatina root
    float x, depth, gainDb;
};
// Classic seating, audience view (negative x = left).
const PresetSlot kOrchestraPreset[16] = {
    {"1st Violins KS.sfz", -0.65f, 0.25f, 0.0f},
    {"2nd Violins KS.sfz", -0.30f, 0.30f, 0.0f},
    {"Violas KS.sfz", 0.25f, 0.30f, 0.0f},
    {"Celli KS.sfz", 0.55f, 0.35f, 0.0f},
    {"Basses KS.sfz", 0.75f, 0.50f, 0.0f},
    {"Flutes KS.sfz", -0.10f, 0.50f, -2.0f},
    {"Oboes KS.sfz", 0.15f, 0.50f, -2.0f},
    {"Clarinets KS.sfz", 0.15f, 0.60f, -2.0f},
    {"Bassoons KS.sfz", -0.10f, 0.60f, -2.0f},
    {"Horns KS.sfz", -0.45f, 0.65f, -1.0f},
    {"Trumpets KS.sfz", 0.30f, 0.70f, -3.0f},
    {"Trombones KS.sfz", 0.45f, 0.70f, -3.0f},
    {"Tuba KS.sfz", 0.60f, 0.72f, -3.0f},
    {"Timpani.sfz", 0.10f, 0.85f, -2.0f},
    {"Concert Harp.sfz", -0.70f, 0.60f, -2.0f},
    {"Mixed Chorus.sfz", 0.00f, 0.90f, -4.0f},
};
} // namespace

juce::File SappOrchestraProcessor::findSonatinaRoot() const
{
    const auto root = settings::samplesRoot();
    // Direct hit first, then a shallow search for the library folder.
    auto direct = root.getChildFile("sonatina").getChildFile("Sonatina Symphonic Orchestra");
    if (direct.isDirectory()) return direct;
    for (const auto& dir : root.findChildFiles(juce::File::findDirectories, true)) {
        if (dir.getFileName() == "Sonatina Symphonic Orchestra") return dir;
    }
    return {};
}

bool SappOrchestraProcessor::orchestraPresetAvailable() const
{
    return findSonatinaRoot().isDirectory();
}

bool SappOrchestraProcessor::loadOrchestraPreset()
{
    if (!orchestraPresetAvailable()) return false;
    const uint64_t generation = ++loadGeneration_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Setting up the orchestra (1/16)...";
    }
    // Seats + balance apply immediately; instruments stream in one by one.
    for (int i = 0; i < 16; ++i) {
        engine_.setSlotStage(i, kOrchestraPreset[i].x, kOrchestraPreset[i].depth, 1.0f);
        engine_.setSlotMix(i, kOrchestraPreset[i].gainDb, false, false);
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
    const auto sonatina = findSonatinaRoot();
    const juce::String fileName(kOrchestraPreset[step].fileName);
    juce::File file;
    for (const auto& candidate :
         sonatina.findChildFiles(juce::File::findFiles, true, fileName))
        if (!candidate.getFullPathName().contains("includes")) { file = candidate; break; }

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
    const uint64_t generation = ++loadGeneration_;
    const int slot = selectedSlot_;
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
        apvts_.replaceState(state);
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
                    const int previous = selectedSlot_;
                    selectedSlot_ = index;
                    loadSfzInstrument(juce::File(path));
                    selectedSlot_ = previous;
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
