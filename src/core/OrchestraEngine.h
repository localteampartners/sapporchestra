#pragma once
// OrchestraEngine — SappOrchestra's product policy wrapped around the generic
// sapp::sounds::PlaybackEngine. MULTITIMBRAL: a 16-slot rack, one slot per
// MIDI channel, all slots seated in one shared room.
//
// Routing: events on MIDI channel N reach slot N (0-based). While only a
// single slot holds an instrument the engine is OMNI — every channel reaches
// that slot — so single-instrument use (and hardware keyboards stuck on
// channel 1) just works. Loading a second slot switches to strict
// per-channel routing.
//
// Per-slot (performance policy, driven per MIDI channel):
//   instrument · articulation keyswitch injection · CC1 dynamics ·
//   CC11 expression · stage position/depth/width (also via SappLink
//   CC16/17/18 on that channel) · early reflections
// Shared (the room and the master):
//   hall FDN tail · early/tail levels · Analog DNA · quality · legato ·
//   master gain · limiter
//
// Framework-independent: no JUCE. The JUCE plugin and the CLI both drive this.

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <sapp/sounds/InstrumentDefinition.h>
#include <sapp/sounds/PlaybackEngine.h>

#include "Reverb.h"

namespace sapp::orchestra {

struct OrchestraParams {
    // Performance defaults (per-channel CC1/CC11 override these live)
    float dynamics = 0.7f;
    float expression = 1.0f;
    float attackTightness = 0.0f; // reserved
    float brightnessMin = 0.12f;
    // Slot-0 stage (CLI/back-compat; the plugin drives setSlotStage directly)
    float stageX = 0.0f;
    float stageDepth = 0.35f;
    float width = 1.0f;
    // Room (shared)
    float earlyLevel = 0.35f;
    float tailLevel = 0.30f;
    float hallSize = 1.0f;
    float hallDecay = 2.6f;
    float hallDamping = 0.45f;
    float hallModulation = 0.35f;
    // Performance policy
    float legato = 1.0f;
    // Analog DNA
    int dnaMode = 1;
    float dnaAmount = 0.18f;
    // SappLink `clean` (CC 3, suite-wide convention — sapptune #30).
    // 0 = every modeled imperfection as designed (the historical behavior);
    // 1 = none. Scales the imperfection sources, never the musical signal.
    float clean = 0.0f;
    // Output
    float masterGainDb = 0.0f;
    bool limiter = true;
    int quality = 1;
};

/// THE `clean` contract for this engine (sapptune #30). Analog DNA is the
/// only modeled-imperfection source SappOrchestra has, and all three of its
/// expressions — per-note random detune, slow gain drift, vintage hiss — are
/// driven by `dnaAmount`, so scaling that one value scales all of them.
/// Audited and deliberately NOT scaled: hall modulation (an FDN device that
/// stops the tail ringing metallically — room design, not wear) and
/// round-robin / velocity variation (which comes from the sample library).
inline float effectiveDnaAmount(const OrchestraParams& p) noexcept
{
    const float clean = p.clean < 0.0f ? 0.0f : (p.clean > 1.0f ? 1.0f : p.clean);
    return p.dnaAmount * (1.0f - clean);
}

class OrchestraEngine {
public:
    static constexpr int kNumSlots = 16;

    OrchestraEngine();
    ~OrchestraEngine();

    // --- control thread -----------------------------------------------------
    void prepare(double sampleRate, int maxBlockFrames);

    void setInstrument(sapp::sounds::InstrumentPtr instrument, int slot = 0);
    void collectRetired();                              // all slots
    sapp::sounds::InstrumentPtr currentInstrument(int slot = 0) const;
    bool slotOccupied(int slot) const;
    int occupiedSlotCount() const;

    // Articulation policy: inject the articulation's keyswitch on that slot.
    void selectArticulation(int index, int slot = 0);

    // Per-slot stage placement (also reachable via CC16/17/18 on the slot's
    // MIDI channel). Thread-safe.
    void setSlotStage(int slot, float x, float depth, float width);
    void getSlotStage(int slot, float& x, float& depth, float& width) const;

    // Per-slot mixer. Solo on any slot silences all non-soloed slots.
    // Thread-safe; changes are smoothed on the audio thread.
    void setSlotMix(int slot, float gainDb, bool mute, bool solo);
    void getSlotMix(int slot, float& gainDb, bool& mute, bool& solo) const;

    void setParams(const OrchestraParams& params);      // shared/room params
    OrchestraParams params() const;

    void resetSequences();
    void reseed(uint32_t seed);

    const sapp::sounds::PlaybackEngine& sampler(int slot = 0) const;
    sapp::sounds::PlaybackEngine& sampler(int slot = 0);

    // --- audio thread -------------------------------------------------------
    // Replaces buffer contents (not additive). Events sorted by frame.
    void process(const sapp::sounds::MidiEvent* events, int eventCount,
                 float* outL, float* outR, int frames) noexcept;

private:
    struct Slot;
    void applyShared(const OrchestraParams& p) noexcept;
    void processSlot(Slot& s, const OrchestraParams& p, bool anySolo,
                     const sapp::sounds::MidiEvent* events, int eventCount,
                     float* outL, float* outR, int frames) noexcept;

    std::vector<std::unique_ptr<Slot>> slots_;
    HallReverb hall_;

    OrchestraParams paramSlots_[2];
    std::atomic<int> paramIndex_{0};

    // Shared smoothed output state.
    float smEarly_ = 0.3f, smTail_ = 0.3f, smMaster_ = 1.0f;
    float dnaPhase_ = 0.0f;
    uint32_t dnaNoise_ = 0x1234567u;

    // Scratch buffers (allocated in prepare).
    std::vector<float> dryL_, dryR_, erTmpL_, erTmpR_, erAccL_, erAccR_,
        sendL_, sendR_, tailL_, tailR_;

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    float lastHallSize_ = -1.0f, lastHallDecay_ = -1.0f, lastHallDamp_ = -1.0f,
          lastHallMod_ = -1.0f;
    int lastQuality_ = -1;
    float lastDnaCents_ = -1.0f;
    int lastLegato_ = -1;
};

} // namespace sapp::orchestra
