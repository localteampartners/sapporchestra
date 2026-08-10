#pragma once
// SappOrchestra plugin processor: JUCE wrapper around OrchestraEngine.
// Owns parameters (APVTS), host state, MIDI conversion, and async
// instrument loading. All sampler/orchestra DSP lives below in
// sapporchestra_core / SappSounds.

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include <juce_audio_utils/juce_audio_utils.h>

#include <sapp/sounds/InstrumentLoader.h>

#include "../core/OrchestraEngine.h"
#include "../core/SfzLibrary.h"

namespace sapporch {

// --- headless operation (sapporchestra #1 / #2) ----------------------------
// Everything below works with no GUI, no user and no message loop.

/// Set SAPP_SFZ_RESCAN=1 to force a full rescan of the samples root at plugin
/// construction, rewriting <root>/.sapp-sfz-index.json before the `instrument`
/// choice list is built. This is the unattended equivalent of the editor's
/// rescan (issue #1) — a station box never opens the editor.
inline constexpr const char* kRescanEnvVar = "SAPP_SFZ_RESCAN";

/// Set SAPP_ORCHESTRA_LOG=<file> to append the plugin's diagnostic lines
/// (SappOrchestra-build / -instrument / -audio-source) to a file. They always
/// go to the host's JUCE logger as well.
inline constexpr const char* kLogEnvVar = "SAPP_ORCHESTRA_LOG";

/// The samples root the library is enumerated from (SAPP_SFZ_ROOT wins over
/// the shared Sapp "samplesRoot" setting), and the index file inside it.
juce::String samplesRootForLibrary();
juce::String indexFilePath();

/// Rescan the samples root and rewrite the index. Returns the entry count, or
/// -1 if the index could not be written. Safe to call before any plugin
/// instance exists — this is what the headless CLI entry points use.
int rebuildSfzIndex();

/// One diagnostic line: JUCE logger (Live's Log.txt et al), stderr on Windows
/// (where the JUCE logger goes to OutputDebugString and nowhere greppable),
/// and $SAPP_ORCHESTRA_LOG when set.
void logLine(const juce::String& message);

class SappOrchestraProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::Timer
{
public:
    SappOrchestraProcessor();
    ~SappOrchestraProcessor() override;

    // --- AudioProcessor -----------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SappOrchestra"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- SappOrchestra ------------------------------------------------------
    juce::AudioProcessorValueTreeState& valueTree() { return apvts_; }
    sapp::orchestra::OrchestraEngine& engine() { return engine_; }

    // Async instrument management (message thread). Loads target the
    // SELECTED slot (one slot per MIDI channel; see OrchestraEngine).
    void loadSfzInstrument(const juce::File& sfzFile);
    void loadSfzInstrumentIntoSlot(const juce::File& sfzFile, int slot);
    void loadDiagnosticInstrument();

    // Host-automatable SFZ selection (sapptune issue #20): the `instrument`
    // AudioParameterChoice enumerates the library scanned at construction
    // (choice 0 = "(keep current)", choice k loads sfzLibrary()[k-1]).
    // The list is FIXED per instance; rescanSfzLibrary() rewrites the index
    // for the NEXT instantiation.
    const std::vector<sapp::sfzlib::Entry>& sfzLibrary() const { return sfzLibrary_; }
    bool rescanSfzLibrary() const;
    juce::String currentInstrumentName() const;
    juce::String currentInstrumentPath() const;
    juce::String loadStatus() const;
    bool isLoading() const { return loading_.load(); }

    /// Readiness signal (sapporchestra #2, mirrors sappkeys v0.8.0). True once
    /// every pending instrument load has landed, so the selected slot holds
    /// what it was told to hold. Mirrored into the `libraryReady` host
    /// parameter so a headless host can POLL instead of guessing a settle.
    bool libraryReady() const;

    // Multitimbral slots (message/UI thread).
    int selectedSlot() const { return selectedSlot_; }
    void selectSlot(int slot);
    bool slotOccupied(int slot) const { return engine_.slotOccupied(slot); }
    juce::String slotName(int slot) const;
    void setSlotMix(int slot, float gainDb, bool mute, bool solo);
    void getSlotMix(int slot, float& gainDb, bool& mute, bool& solo) const;

    // Factory presets: a full orchestra across all 16 channels, seated on
    // the stage, loading sequentially in the background. Preset 0 = Sonatina,
    // 1 = Virtual Playing Orchestra. Returns false when the library is not
    // found in the samples folder.
    bool loadOrchestraPreset(int preset = 0);
    bool orchestraPresetAvailable(int preset = 0) const;
    int orchestraPresetCount() const;
    juce::String orchestraPresetName(int preset) const;

    // Articulations of the loaded instrument (message/UI thread).
    juce::StringArray articulationNames() const;
    int currentArticulation() const;
    void selectArticulation(int index);

    juce::MidiKeyboardState keyboardState;

    std::function<void()> onInstrumentChanged;  // editor hook (message thread)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
        makeLayout(std::vector<sapp::sfzlib::Entry>& outLibrary);
    void pushParamsToEngine();
    juce::File findLibraryDir(const juce::String& dirName) const;

    // --- instrument loading (sapporchestra #2) ------------------------------
    // EVERY instrument install happens on loaderThread_, never on the JUCE
    // message thread. A VST3 plugin inside a non-JUCE headless host has a
    // MessageManager that nobody pumps: juce::Timer callbacks and
    // MessageManager::callAsync() never run there, so a load routed through
    // them is accepted and then silently dropped — which is exactly how the
    // `instrument` parameter came to be a no-op on the station box. The 30 Hz
    // timer survives only as an EDITOR convenience and nothing depends on it.
    struct LoadJob {
        enum class Kind { Sfz, Diagnostic, PresetStep };
        Kind kind = Kind::Sfz;
        juce::String path;          // Sfz: the file to load
        int slot = 0;
        uint64_t generation = 0;
        int presetStep = -1;        // PresetStep: which of the 16 slots
        bool constructionDefault = false;  // the diagnostic the ctor installs
        bool syncParameter = true;  // reflect the result into `instrument`
    };
    void loaderLoop();
    void enqueueLoad(LoadJob job);
    void performLoad(LoadJob job);
    void performPresetStep(const LoadJob& job);
    void finishLoad(sapp::sounds::LoadResult result, const juce::String& path,
                    const LoadJob& job);
    void publishReadiness();
    void logInstalled(const juce::String& what, int slot, bool ok);
    void logAudioSourceIfNeeded();

    // --- `instrument` choice parameter plumbing (sapptune issue #20) --------
    // parameterChanged may fire on the audio thread: it only stores an index;
    // the loader thread picks it up (SFZ loads must never run on the audio
    // thread, and must never depend on the host pumping a message loop).
    void parameterChanged(const juce::String& parameterId, float newValue) override;
    void timerCallback() override;
    void applyInstrumentChoice(int choiceIndex);
    void applyProgramSelect(int slot, int entryIndex);
    // Reflect a loaded path back into the parameter without re-triggering a
    // load (guarded). "" or an unknown path selects choice 0.
    void syncInstrumentParameter(const juce::String& path);

    int activePreset_ = 0;
    mutable std::mutex libraryCacheMutex_;
    mutable std::map<juce::String, std::pair<juce::File, juce::uint32>> libraryRootCache_;

    // Library snapshot behind the `instrument` choice list. Declared BEFORE
    // apvts_: makeLayout(sfzLibrary_) fills it while building the layout.
    std::vector<sapp::sfzlib::Entry> sfzLibrary_;

    juce::AudioProcessorValueTreeState apvts_;
    sapp::orchestra::OrchestraEngine engine_;

    // Cached raw parameter pointers (audio thread reads).
    std::atomic<float>* pDynamics_ = nullptr;
    std::atomic<float>* pExpression_ = nullptr;
    std::atomic<float>* pStageX_ = nullptr;
    std::atomic<float>* pStageDepth_ = nullptr;
    std::atomic<float>* pWidth_ = nullptr;
    std::atomic<float>* pEarly_ = nullptr;
    std::atomic<float>* pTail_ = nullptr;
    std::atomic<float>* pHallSize_ = nullptr;
    std::atomic<float>* pHallDecay_ = nullptr;
    std::atomic<float>* pHallDamping_ = nullptr;
    std::atomic<float>* pLegato_ = nullptr;
    std::atomic<float>* pDnaMode_ = nullptr;
    std::atomic<float>* pDnaAmount_ = nullptr;
    std::atomic<float>* pMaster_ = nullptr;
    std::atomic<float>* pLimiter_ = nullptr;
    std::atomic<float>* pQuality_ = nullptr;
    std::atomic<float>* pArticulation_ = nullptr;
    std::atomic<float>* pClean_ = nullptr;

    // Readiness readout (sapporchestra #2). Deliberately OUTSIDE the APVTS:
    // it is a status signal, not part of the sound, so copyState/replaceState
    // must never save or restore it — a stale "ready" from a saved session
    // would lie. Non-automatable, appended last so no index moves.
    juce::AudioParameterBool* libraryReady_ = nullptr;

    // Knob→CC bridging: moving Dynamics/Expression injects the matching CC.
    float lastDynParam_ = -1.0f, lastExprParam_ = -1.0f;
    int lastArticulationParam_ = -1;

    // SappLink CC-in (see src/core/SappLinkCCMap.h): mapped controllers land
    // as slew targets; each block moves the APVTS parameter a fraction of the
    // way — the same normalized path host automation uses — so 7-bit CC steps
    // don't zipper. CC 1/11/64 are engine-native and never appear here.
    struct CcSlew {
        juce::RangedAudioParameter* parameter = nullptr;
        float target = 0.0f, current = 0.0f;
        bool active = false;
    };
    std::array<CcSlew, 12> ccSlews_;
    void handleSappLinkCc(int ccNumber, int ccValue);
    void advanceCcSlews(int numSamples);

    std::vector<sapp::sounds::MidiEvent> eventScratch_;

    // `instrument` choice apply state. pendingInstrumentChoice_ targets the
    // selected slot; pendingProgramSelect_ packs (channel << 16) | entry from
    // MIDI bank-select + program change. -1 = nothing pending.
    std::atomic<int> pendingInstrumentChoice_{-1};
    std::atomic<int> pendingProgramSelect_{-1};
    std::array<uint8_t, 16> bankMsb_{};   // CC0 per channel (audio thread only)
    std::array<uint8_t, 16> bankLsb_{};   // CC32 per channel (audio thread only)
    std::atomic<bool> applyingInstrumentChoice_{false};  // reentry guard

    std::atomic<int> selectedSlot_{0};
    std::array<juce::String, 16> slotPaths_;   // "" = empty / diagnostic
    std::array<juce::String, 16> slotNames_;
    float lastStageX_ = -99.0f, lastStageDepth_ = -99.0f, lastWidth_ = -99.0f;
    juce::String loadStatus_{"starting"};
    std::atomic<bool> loading_{false};
    // loadGeneration_ hands out unique tickets; the guard that decides whether
    // a finished load may still install is PER SLOT (a 16-slot state restore
    // queues 16 loads, and a single global generation would let only the last
    // one land). presetGeneration_ keeps a factory-preset chain alive until a
    // newer preset supersedes it.
    std::atomic<uint64_t> loadGeneration_{0};
    std::array<std::atomic<uint64_t>, 16> slotGeneration_{};
    std::atomic<uint64_t> presetGeneration_{0};
    juce::CriticalSection loadLock_;

    // Loader thread + its queue (see LoadJob above).
    std::deque<LoadJob> loadQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread loaderThread_;
    std::atomic<bool> loaderStop_{false};
    std::atomic<int> jobsOutstanding_{0};
    std::atomic<uint64_t> installCount_{0};
    std::atomic<bool> instrumentChangedFlag_{false};  // editor hook, via timer

    // SappOrchestra-audio-source: names WHAT sounded, so a wrong-instrument
    // render is visible in a log instead of only by ear. The audio thread
    // flags a voice batch that started from silence; the loader thread logs.
    std::atomic<bool> audioBatchStarted_{false};
    int lastVoiceCount_ = 0;                  // audio thread only
    double lastAudioSourceLogMs_ = 0.0;       // loader thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappOrchestraProcessor)
};

} // namespace sapporch
