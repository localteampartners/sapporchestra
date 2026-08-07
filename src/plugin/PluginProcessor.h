#pragma once
// SappOrchestra plugin processor: JUCE wrapper around OrchestraEngine.
// Owns parameters (APVTS), host state, MIDI conversion, and async
// instrument loading. All sampler/orchestra DSP lives below in
// sapporchestra_core / SappSounds.

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include <juce_audio_utils/juce_audio_utils.h>

#include <sapp/sounds/InstrumentLoader.h>

#include "../core/OrchestraEngine.h"

namespace sapporch {

class SappOrchestraProcessor : public juce::AudioProcessor
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
    void loadDiagnosticInstrument();
    juce::String currentInstrumentName() const;
    juce::String currentInstrumentPath() const { return slotPaths_[size_t(selectedSlot_)]; }
    juce::String loadStatus() const;
    bool isLoading() const { return loading_.load(); }

    // Multitimbral slots (message/UI thread).
    int selectedSlot() const { return selectedSlot_; }
    void selectSlot(int slot);
    bool slotOccupied(int slot) const { return engine_.slotOccupied(slot); }
    juce::String slotName(int slot) const;
    void setSlotMix(int slot, float gainDb, bool mute, bool solo);
    void getSlotMix(int slot, float& gainDb, bool& mute, bool& solo) const;

    // Factory preset: full Sonatina orchestra across all 16 channels,
    // seated on the stage. Loads sequentially in the background.
    // Returns false when Sonatina is not found in the samples folder.
    bool loadOrchestraPreset();
    bool orchestraPresetAvailable() const;

    // Articulations of the loaded instrument (message/UI thread).
    juce::StringArray articulationNames() const;
    int currentArticulation() const;
    void selectArticulation(int index);

    juce::MidiKeyboardState keyboardState;

    std::function<void()> onInstrumentChanged;  // editor hook (message thread)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushParamsToEngine();
    void finishLoad(sapp::sounds::LoadResult result, const juce::String& path,
                    uint64_t generation, int slot);
    void loadOrchestraPresetStep(size_t step, uint64_t generation);
    juce::File findSonatinaRoot() const;

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
    std::array<CcSlew, 11> ccSlews_;
    void handleSappLinkCc(int ccNumber, int ccValue);
    void advanceCcSlews(int numSamples);

    std::vector<sapp::sounds::MidiEvent> eventScratch_;

    int selectedSlot_ = 0;
    std::array<juce::String, 16> slotPaths_;   // "" = empty / diagnostic
    std::array<juce::String, 16> slotNames_;
    float lastStageX_ = -99.0f, lastStageDepth_ = -99.0f, lastWidth_ = -99.0f;
    juce::String loadStatus_{"starting"};
    std::atomic<bool> loading_{false};
    std::atomic<uint64_t> loadGeneration_{0};
    juce::CriticalSection loadLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappOrchestraProcessor)
};

} // namespace sapporch
