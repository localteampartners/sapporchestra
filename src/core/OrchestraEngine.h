#pragma once
// OrchestraEngine — SappOrchestra's product policy wrapped around the generic
// sapp::sounds::PlaybackEngine.
//
// SappSounds owns: SFZ, samples, voices, mapping, round robin, keyswitches.
// SappOrchestra owns (here): CC1 dynamics (level + timbre), CC11 expression,
// stage placement (pan/width/depth), early reflections + shared hall,
// Analog DNA character, quality modes, master output policy.
//
// Framework-independent: no JUCE. The JUCE plugin and the CLI both drive this.

#include <atomic>
#include <cstdint>
#include <memory>

#include <sapp/sounds/InstrumentDefinition.h>
#include <sapp/sounds/PlaybackEngine.h>

#include "Reverb.h"

namespace sapp::orchestra {

struct OrchestraParams {
    // Performance
    float dynamics = 0.7f;        // 0..1, follows CC1
    float expression = 1.0f;      // 0..1, follows CC11
    // Tone
    float attackTightness = 0.0f; // reserved (region policy), 0..1
    float brightnessMin = 0.12f;  // LP mapping floor at pp
    // Stage
    float stageX = 0.0f;          // -1 left .. +1 right
    float stageDepth = 0.35f;     // 0 close .. 1 far
    float width = 1.0f;           // 0 mono .. 2 wide
    // Room
    float earlyLevel = 0.35f;
    float tailLevel = 0.30f;
    float hallSize = 1.0f;        // 0.2..1.5
    float hallDecay = 2.6f;       // seconds
    float hallDamping = 0.45f;
    float hallModulation = 0.35f;
    // Analog DNA
    int dnaMode = 1;              // 0 clean, 1 cohesive, 2 vintage
    float dnaAmount = 0.18f;
    // Output
    float masterGainDb = 0.0f;
    bool limiter = true;
    int quality = 1;              // 0 draft, 1 normal/high
};

class OrchestraEngine {
public:
    OrchestraEngine();

    // --- control thread -----------------------------------------------------
    void prepare(double sampleRate, int maxBlockFrames);
    void setInstrument(sapp::sounds::InstrumentPtr instrument);
    void collectRetired();
    sapp::sounds::InstrumentPtr currentInstrument() const;

    // Articulation policy: press the articulation's keyswitch on the next block.
    // Index into instrument definition's articulations. Thread-safe.
    void selectArticulation(int index);

    void setParams(const OrchestraParams& params);   // copied atomically
    OrchestraParams params() const;

    void resetSequences();
    void reseed(uint32_t seed);

    const sapp::sounds::PlaybackEngine& sampler() const { return sampler_; }
    sapp::sounds::PlaybackEngine& sampler() { return sampler_; }

    // --- audio thread -------------------------------------------------------
    // Replaces buffer contents (not additive). Events sorted by frame.
    void process(const sapp::sounds::MidiEvent* events, int eventCount,
                 float* outL, float* outR, int frames) noexcept;

private:
    void applyQuality(const OrchestraParams& p) noexcept;

    sapp::sounds::PlaybackEngine sampler_;
    EarlyReflections early_;
    HallReverb hall_;

    // Double-buffered params: control writes inactive slot then flips.
    OrchestraParams paramSlots_[2];
    std::atomic<int> paramIndex_{0};

    std::atomic<int> pendingArticulationKeyswitch_{-1};

    // Live controller state (audio thread): CC1/CC11 override the params once
    // received, so a ridden mod-wheel phrase persists across blocks. -1 = no
    // CC received yet → the parameter value applies.
    float liveDynamics_ = -1.0f, liveExpression_ = -1.0f;

    // Smoothed audio-thread state.
    float smDynGain_ = 0.5f, smExprGain_ = 1.0f, smCutoffCoef_ = 1.0f;
    float smPanL_ = 1.0f, smPanR_ = 1.0f, smDirect_ = 1.0f;
    float smEarly_ = 0.3f, smTail_ = 0.3f, smMaster_ = 1.0f;
    float lpL_ = 0.0f, lpR_ = 0.0f;      // dynamics timbre filter
    float depthLpL_ = 0.0f, depthLpR_ = 0.0f;  // distance damping
    float dnaPhase_ = 0.0f;
    uint32_t dnaNoise_ = 0x1234567u;

    // Scratch buffers (allocated in prepare).
    std::vector<float> dryL_, dryR_, sendL_, sendR_, earlyL_, earlyR_, tailL_, tailR_;

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    float lastHallSize_ = -1.0f, lastHallDecay_ = -1.0f, lastHallDamp_ = -1.0f,
          lastHallMod_ = -1.0f, lastPredelay_ = -1.0f, lastStageX_ = -99.0f,
          lastDepthForEarly_ = -1.0f;
    int lastQuality_ = -1;
    float lastDnaCents_ = -1.0f;
};

} // namespace sapp::orchestra
