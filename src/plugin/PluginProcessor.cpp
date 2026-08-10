#include "PluginProcessor.h"

#include <chrono>
#include <cstdio>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "../core/SappLinkCCMap.h"
#include "PluginEditor.h"
#include "SappSettings.h"

#ifndef SAPPORCH_VERSION
#define SAPPORCH_VERSION "0.0.0"
#endif

namespace sapporch {

using namespace sapp::orchestra;
using sapp::sounds::MidiEvent;

namespace {
constexpr int kMaxArticulations = 16;

bool envFlagSet(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != 0 && juce::String(value) != "0";
}
} // namespace

// ------------------------------------------------- headless entry points --

juce::String samplesRootForLibrary()
{
    return juce::String(sapp::sfzlib::resolveRoot(
        settings::samplesRoot().getFullPathName().toStdString()));
}

juce::String indexFilePath()
{
    return juce::File(samplesRootForLibrary())
        .getChildFile(sapp::sfzlib::kIndexFileName)
        .getFullPathName();
}

int rebuildSfzIndex()
{
    const auto root = samplesRootForLibrary().toStdString();
    const auto entries = sapp::sfzlib::scan(root);
    if (!sapp::sfzlib::writeIndex(root, entries)) return -1;
    return int(entries.size());
}

void logLine(const juce::String& message)
{
    juce::Logger::writeToLog(message);
#if JUCE_WINDOWS
    // JUCE's fallback logger is OutputDebugString on Windows — invisible to a
    // station box redirecting the host's output. stderr is what it greps.
    std::fputs((message + "\n").toRawUTF8(), stderr);
    std::fflush(stderr);
#endif
    if (const char* path = std::getenv(kLogEnvVar))
        if (path[0] != 0)
            juce::File(juce::String::fromUTF8(path)).appendText(message + "\n");
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
    //
    // SAPP_SFZ_RESCAN=1 rebuilds the index first (issue #1): the editor's
    // rescan can never run on a station box, so the unattended path is an
    // environment variable read right here, before the list is built.
    const auto root = samplesRootForLibrary().toStdString();
    if (envFlagSet(kRescanEnvVar)) {
        outLibrary = sapp::sfzlib::scan(root);
        sapp::sfzlib::writeIndex(root, outLibrary);
        logLine("SappOrchestra-sfz-index: rescan=1 root=\"" + juce::String(root)
                + "\" entries=" + juce::String(int(outLibrary.size())));
    } else {
        outLibrary = sapp::sfzlib::loadOrScan(root);
    }
    juce::StringArray instrumentChoices;
    instrumentChoices.add("(keep current)");
    for (const auto& entry : outLibrary)
        instrumentChoices.add(juce::String::fromUTF8(entry.label.c_str()));
    if (outLibrary.empty())
        instrumentChoices.add("(no SFZ library found)");  // avoid a 1-step param
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"instrument", 1}, "Instrument", instrumentChoices, 0));

    // APPENDED LAST AGAIN (sapptune #30): the suite-wide `clean` control.
    // 0 = every modeled imperfection exactly as today (backwards compatible).
    layout.add(std::make_unique<P>(juce::ParameterID{"clean", 1}, "Clean",
                                   Range{0.0f, 1.0f, 0.001f}, 0.0f));
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
    pClean_ = raw("clean");

    eventScratch_.reserve(512);

    const auto& table = sapplink::mappings();
    static_assert(std::tuple_size<decltype(ccSlews_)>::value == size_t(sapplink::kNumMappings),
                  "ccSlews_ must match the SappLink mapping table size");
    for (size_t i = 0; i < table.size(); ++i)
        ccSlews_[i].parameter = apvts_.getParameter(table[i].paramId);

    // Readiness readout (sapporchestra #2): a headless host polls this instead
    // of guessing a settle window. Outside the APVTS on purpose — see the
    // declaration. Appended last, after every APVTS parameter.
    libraryReady_ = new juce::AudioParameterBool(
        juce::ParameterID{"libraryReady", 1}, "Library Ready", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false));
    addParameter(libraryReady_);

    // Host-automatable SFZ selection: the callback may fire on the audio
    // thread, so it only stores an index; the LOADER THREAD applies it.
    apvts_.addParameterListener("instrument", this);

    // The loader thread owns every instrument install. Started before the
    // construction diagnostic is queued so nothing waits on the host.
    loaderThread_ = std::thread([this] { loaderLoop(); });

    // The 30 Hz timer is an editor convenience only (it fires the
    // onInstrumentChanged hook on the message thread). NOTHING about loading
    // depends on it — see the LoadJob comment in the header.
    startTimerHz(30);

    // Which build did the host just load, and what is it enumerating? One
    // line at construction turns "the wrong sound came out" from guesswork
    // into a log grep (sappkeys #1 taught this the hard way).
    logLine("SappOrchestra-build: version=" SAPPORCH_VERSION
            " root=\"" + samplesRootForLibrary()
            + "\" instruments=" + juce::String(int(sfzLibrary_.size())));

    loadDiagnosticInstrument();
}

// --------------------------------------------- `instrument` choice param --

void SappOrchestraProcessor::parameterChanged(const juce::String& parameterId,
                                              float newValue)
{
    if (parameterId != juce::StringRef("instrument")
        || applyingInstrumentChoice_.load(std::memory_order_acquire))
        return;
    pendingInstrumentChoice_.store(int(newValue + 0.5f));
    // Not ready from the instant the host asks for a different instrument —
    // otherwise a host that writes the parameter and immediately polls would
    // read the PREVIOUS instrument's "ready" and render too early.
    if (libraryReady_ != nullptr && libraryReady_->get())
        *libraryReady_ = false;
    queueCv_.notify_all();
}

void SappOrchestraProcessor::timerCallback()
{
    // Editor convenience ONLY. Loading does not run here (see LoadJob).
    if (instrumentChangedFlag_.exchange(false) && onInstrumentChanged)
        onInstrumentChanged();
}

// The loader thread: the one place instruments are installed. Runs whether or
// not the host has a message loop, which is the entire point (issue #2).
void SappOrchestraProcessor::loaderLoop()
{
    while (!loaderStop_.load(std::memory_order_acquire)) {
        // MIDI program select first, then an explicit parameter move: when
        // both land in the same tick the parameter (the deliberate host move)
        // wins because it is enqueued last.
        const int programSelect = pendingProgramSelect_.exchange(-1);
        if (programSelect >= 0)
            applyProgramSelect(programSelect >> 16, programSelect & 0xffff);
        const int choice = pendingInstrumentChoice_.exchange(-1);
        if (choice >= 0)
            applyInstrumentChoice(choice);

        LoadJob job;
        bool haveJob = false;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (!loadQueue_.empty()) {
                job = std::move(loadQueue_.front());
                loadQueue_.pop_front();
                haveJob = true;
            }
        }
        if (haveJob) {
            performLoad(std::move(job));
            jobsOutstanding_.fetch_sub(1);
            loading_.store(jobsOutstanding_.load() > 0);
            publishReadiness();
            continue;
        }

        loading_.store(false);
        publishReadiness();
        logAudioSourceIfNeeded();
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCv_.wait_for(lock, std::chrono::milliseconds(5));
    }
}

void SappOrchestraProcessor::enqueueLoad(LoadJob job)
{
    jobsOutstanding_.fetch_add(1);
    loading_.store(true);
    if (libraryReady_ != nullptr && libraryReady_->get())
        *libraryReady_ = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        loadQueue_.push_back(std::move(job));
    }
    queueCv_.notify_all();
}

void SappOrchestraProcessor::performLoad(LoadJob job)
{
    if (job.kind != LoadJob::Kind::PresetStep
        && job.generation != slotGeneration_[size_t(job.slot)].load())
        return;  // a newer load for this slot was queued before we started

    if (job.kind == LoadJob::Kind::Diagnostic) {
        sapp::sounds::LoadResult result;
        result.instrument = sapp::sounds::makeDiagnosticInstrument();
        result.ok = result.instrument != nullptr;
        finishLoad(std::move(result), {}, job);
        return;
    }

    if (job.kind == LoadJob::Kind::PresetStep) {
        performPresetStep(job);
        return;
    }

    sapp::sounds::InstrumentLoader loader;
    auto result = loader.loadSfz(job.path.toStdString());
    finishLoad(std::move(result), job.path, job);
}

void SappOrchestraProcessor::applyInstrumentChoice(int choiceIndex)
{
    if (choiceIndex <= 0 || choiceIndex > int(sfzLibrary_.size())) {
        // "(keep current)" / placeholder / out of range. Nothing to load, but
        // a host that asked for it is entitled to a readiness edge.
        publishReadiness();
        return;
    }
    const auto& entry = sfzLibrary_[size_t(choiceIndex - 1)];
    const juce::File file(juce::String::fromUTF8(entry.path.c_str()));
    if (file.getFullPathName() == currentInstrumentPath())
        return;  // already loaded in the selected slot
    if (!file.existsAsFile()) {
        // Graceful miss: the library changed since the index was written.
        {
            const juce::ScopedLock sl(loadLock_);
            loadStatus_ = "Missing: " + file.getFullPathName();
        }
        logLine("SappOrchestra-instrument: MISSING choice=" + juce::String(choiceIndex)
                + " path=\"" + file.getFullPathName() + "\" (index is stale - run "
                  "`sapporchestra sfz-index --rescan` or set SAPP_SFZ_RESCAN=1)");
        instrumentChangedFlag_.store(true);
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
    {
        const juce::ScopedLock sl(loadLock_);
        if (file.getFullPathName() == slotPaths_[size_t(slot)])
            return;
    }
    if (!file.existsAsFile()) {
        {
            const juce::ScopedLock sl(loadLock_);
            loadStatus_ = "Missing: " + file.getFullPathName();
        }
        logLine("SappOrchestra-instrument: MISSING program entry=" + juce::String(entryIndex)
                + " path=\"" + file.getFullPathName() + "\"");
        instrumentChangedFlag_.store(true);
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
    applyingInstrumentChoice_.store(true, std::memory_order_release);
    parameter->setValueNotifyingHost(
        parameter->convertTo0to1(float(choice)));
    applyingInstrumentChoice_.store(false, std::memory_order_release);
}

void SappOrchestraProcessor::publishReadiness()
{
    if (libraryReady_ == nullptr) return;
    const bool ready = jobsOutstanding_.load() == 0
                       && pendingInstrumentChoice_.load() < 0
                       && pendingProgramSelect_.load() < 0
                       && installCount_.load() > 0;
    if (libraryReady_->get() != ready)
        *libraryReady_ = ready;
}

void SappOrchestraProcessor::logInstalled(const juce::String& what, int slot, bool ok)
{
    logLine(juce::String("SappOrchestra-instrument: ") + (ok ? "loaded" : "FAILED")
            + " slot=" + juce::String(slot)
            + " source=\"" + what + "\" build=" SAPPORCH_VERSION);
}

void SappOrchestraProcessor::logAudioSourceIfNeeded()
{
    // Voices started from silence: name the instrument that produced them.
    // This is the line that makes "the plugin is playing its default sound"
    // visible in the wild instead of only audible (sappkeys #1 / sapptune #21).
    if (!audioBatchStarted_.exchange(false)) return;
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    if (nowMs - lastAudioSourceLogMs_ < 3000.0) return;
    lastAudioSourceLogMs_ = nowMs;

    const int slot = selectedSlot_.load();
    juce::String source, name;
    {
        const juce::ScopedLock sl(loadLock_);
        source = slotPaths_[size_t(slot)];
        name = slotNames_[size_t(slot)];
    }
    if (source.isEmpty())
        // ASCII only: these lines end up in host logs with every encoding.
        source = "DIAGNOSTIC(no SFZ loaded - the built-in default is sounding)";
    logLine("SappOrchestra-audio-source: instrument=\"" + source
            + "\" name=\"" + name + "\" slot=" + juce::String(slot)
            + " build=" SAPPORCH_VERSION
            + " ready=" + juce::String(libraryReady() ? 1 : 0));
}

bool SappOrchestraProcessor::libraryReady() const
{
    return libraryReady_ != nullptr && libraryReady_->get();
}

bool SappOrchestraProcessor::rescanSfzLibrary() const
{
    return rebuildSfzIndex() >= 0;
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
    // Join the loader before anything it touches is destroyed. (The old
    // MessageManager::callAsync design could not do this: closures capturing
    // `this` outlived the instance and crashed when a later pump ran them.)
    loaderStop_.store(true, std::memory_order_release);
    queueCv_.notify_all();
    if (loaderThread_.joinable()) loaderThread_.join();
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
        engine_.setSlotStage(selectedSlot_.load(), p.stageX, p.stageDepth, p.width);
    }
    p.earlyLevel = pEarly_->load();
    p.tailLevel = pTail_->load();
    p.hallSize = pHallSize_->load();
    p.hallDecay = pHallDecay_->load();
    p.hallDamping = pHallDamping_->load();
    p.legato = pLegato_->load();
    p.dnaMode = int(pDnaMode_->load());
    p.dnaAmount = pDnaAmount_->load();
    // SappLink `clean` (CC 3): scales every modeled imperfection by (1-clean).
    p.clean = pClean_ != nullptr ? pClean_->load() : 0.0f;
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
        engine_.selectArticulation(artParam, selectedSlot_.load());
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

    // Silence → voices: flag it so the loader thread names what just sounded.
    int voices = 0;
    for (int slot = 0; slot < sapp::orchestra::OrchestraEngine::kNumSlots; ++slot)
        if (engine_.slotOccupied(slot)) voices += engine_.sampler(slot).activeVoiceCount();
    if (voices > 0 && lastVoiceCount_ == 0)
        audioBatchStarted_.store(true, std::memory_order_relaxed);
    lastVoiceCount_ = voices;

    midi.clear();
}

// ------------------------------------------------------------- instruments --

void SappOrchestraProcessor::loadDiagnosticInstrument()
{
    LoadJob job;
    job.kind = LoadJob::Kind::Diagnostic;
    job.slot = juce::jlimit(0, 15, selectedSlot_.load());
    job.generation = ++loadGeneration_;
    slotGeneration_[size_t(job.slot)].store(job.generation);
    // The construction diagnostic must NOT write the `instrument` parameter:
    // a host that has already selected an instrument would see its choice
    // reset to "(keep current)" when this landed.
    job.constructionDefault = installCount_.load() == 0;
    job.syncParameter = !job.constructionDefault;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Generating diagnostic orchestra...";
    }
    enqueueLoad(std::move(job));
}

void SappOrchestraProcessor::selectSlot(int slot)
{
    slot = juce::jlimit(0, sapp::orchestra::OrchestraEngine::kNumSlots - 1, slot);
    if (slot == selectedSlot_.load()) return;
    selectedSlot_.store(slot);

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
    juce::String path;
    {
        const juce::ScopedLock sl(loadLock_);
        path = slotPaths_[size_t(slot)];
    }
    syncInstrumentParameter(path);
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
    // large samples tree is not free. Read from the UI timer and the loader
    // thread both, hence the mutex.
    const auto now = juce::Time::getMillisecondCounter();
    {
        std::lock_guard<std::mutex> lock(libraryCacheMutex_);
        auto it = libraryRootCache_.find(dirName);
        if (it != libraryRootCache_.end() && now - it->second.second < 10000)
            return it->second.first;
    }
    const auto root = settings::samplesRoot();
    juce::File found;
    if (root.getChildFile(dirName).isDirectory())
        found = root.getChildFile(dirName);
    else
        for (const auto& dir : root.findChildFiles(juce::File::findDirectories, true))
            if (dir.getFileName() == dirName) { found = dir; break; }
    {
        std::lock_guard<std::mutex> lock(libraryCacheMutex_);
        libraryRootCache_[dirName] = {found, now};
    }
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
    presetGeneration_.store(generation);
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Setting up the orchestra (1/16)...";
    }
    // Seats + balance apply immediately; instruments stream in one by one on
    // the loader thread (one queued PresetStep job chaining to the next).
    const auto& def = kOrchestraPresets[preset];
    for (int i = 0; i < 16; ++i) {
        engine_.setSlotStage(i, def.slots[i].x, def.slots[i].depth, 1.0f);
        engine_.setSlotMix(i, def.slots[i].gainDb, false, false);
    }
    selectedSlot_.store(-1);   // force the slot-0 reselect to reflect its seat
    selectSlot(0);
    LoadJob job;
    job.kind = LoadJob::Kind::PresetStep;
    job.generation = generation;
    job.presetStep = 0;
    enqueueLoad(std::move(job));
    return true;
}

void SappOrchestraProcessor::performPresetStep(const LoadJob& job)
{
    if (job.generation != presetGeneration_.load())
        return;  // a newer preset took over: abandon this chain
    const size_t step = size_t(job.presetStep);
    if (step >= 16) {
        {
            const juce::ScopedLock sl(loadLock_);
            loadStatus_ = "Full orchestra ready - 16 channels";
        }
        instrumentChangedFlag_.store(true);
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
    instrumentChangedFlag_.store(true);

    if (file.existsAsFile()) {
        LoadJob slotJob = job;
        slotJob.slot = int(step);
        slotGeneration_[step].store(job.generation);
        sapp::sounds::InstrumentLoader loader;
        auto result = loader.loadSfz(file.getFullPathName().toStdString());
        finishLoad(std::move(result), file.getFullPathName(), slotJob);
    }

    LoadJob next = job;
    next.presetStep = int(step) + 1;
    enqueueLoad(std::move(next));
}

juce::String SappOrchestraProcessor::slotName(int slot) const
{
    if (slot < 0 || slot >= 16) return {};
    const juce::ScopedLock sl(loadLock_);
    return slotNames_[size_t(slot)];
}

void SappOrchestraProcessor::loadSfzInstrument(const juce::File& sfzFile)
{
    loadSfzInstrumentIntoSlot(sfzFile, selectedSlot_.load());
}

void SappOrchestraProcessor::loadSfzInstrumentIntoSlot(const juce::File& sfzFile, int slot)
{
    LoadJob job;
    job.kind = LoadJob::Kind::Sfz;
    job.path = sfzFile.getFullPathName();
    job.slot = juce::jlimit(0, 15, slot);
    job.generation = ++loadGeneration_;
    slotGeneration_[size_t(job.slot)].store(job.generation);
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Loading " + sfzFile.getFileName() + "...";
    }
    enqueueLoad(std::move(job));
}

// Loader thread. Installs the instrument and publishes everything that
// depends on it; the editor is notified later, from the timer.
void SappOrchestraProcessor::finishLoad(sapp::sounds::LoadResult result,
                                        const juce::String& path, const LoadJob& job)
{
    // Superseded loads must NOT install. (This guard existed but never
    // returned, so the slow construction-time diagnostic could land after a
    // real SFZ and overwrite it — half of issue #2.) Per slot, so a 16-slot
    // state restore installs all sixteen.
    const int slot = job.slot;
    if (job.generation != slotGeneration_[size_t(slot)].load())
        return;
    bool ok = false;
    {
        const juce::ScopedLock sl(loadLock_);
        if (!result.ok || result.instrument == nullptr) {
            loadStatus_ = "Load failed";
            for (const auto& d : result.diagnostics)
                if (d.severity == sapp::sounds::Severity::Error) {
                    loadStatus_ = "Load failed: " + juce::String(d.message);
                    break;
                }
        } else {
            ok = true;
            engine_.setInstrument(result.instrument, slot);
            engine_.collectRetired();
            slotPaths_[size_t(slot)] = path;
            slotNames_[size_t(slot)] = juce::String(result.instrument->definition.name);
            loadStatus_ = result.missingSamples.empty()
                              ? "Ready"
                              : juce::String(result.missingSamples.size()) + " samples missing";
            lastArticulationParam_ = -1;  // re-apply articulation on next block
        }
    }
    if (ok) installCount_.fetch_add(1);
    logInstalled(path.isEmpty() ? juce::String("DIAGNOSTIC") : path, slot, ok);

    // Reflect the selected slot's instrument in the `instrument` parameter
    // (guarded — this must not schedule another load). Never for the
    // construction diagnostic: it would wipe a selection the host already made.
    if (job.syncParameter && slot == selectedSlot_.load()) {
        juce::String current;
        {
            const juce::ScopedLock sl(loadLock_);
            current = slotPaths_[size_t(slot)];
        }
        syncInstrumentParameter(current);
    }
    instrumentChangedFlag_.store(true);
}

juce::String SappOrchestraProcessor::currentInstrumentName() const
{
    const juce::ScopedLock sl(loadLock_);
    const auto name = slotNames_[size_t(juce::jmax(0, selectedSlot_.load()))];
    return name.isEmpty() ? "(empty)" : name;
}

juce::String SappOrchestraProcessor::currentInstrumentPath() const
{
    const juce::ScopedLock sl(loadLock_);
    return slotPaths_[size_t(juce::jmax(0, selectedSlot_.load()))];
}

juce::String SappOrchestraProcessor::loadStatus() const
{
    const juce::ScopedLock sl(loadLock_);
    return loadStatus_;
}

juce::StringArray SappOrchestraProcessor::articulationNames() const
{
    juce::StringArray names;
    if (auto inst = engine_.currentInstrument(selectedSlot_.load()))
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
    state.setProperty("selectedSlot", selectedSlot_.load(), nullptr);
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
        applyingInstrumentChoice_.store(true, std::memory_order_release);
        apvts_.replaceState(state);
        applyingInstrumentChoice_.store(false, std::memory_order_release);
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
