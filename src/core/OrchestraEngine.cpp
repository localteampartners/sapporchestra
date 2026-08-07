#include "OrchestraEngine.h"

#include <algorithm>
#include <cmath>

#include "SappLinkCCMap.h"

namespace sapp::orchestra {

using sapp::sounds::MidiEvent;

namespace {
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }

inline float smoothCoef(double sampleRate, float ms) noexcept
{
    return 1.0f - std::exp(-1.0f / (float(sampleRate) * ms * 0.001f));
}

constexpr int kMaxSlotEvents = 512;
} // namespace

// -------------------------------------------------------------------- slot --

struct OrchestraEngine::Slot {
    sapp::sounds::PlaybackEngine sampler;
    EarlyReflections early;

    std::atomic<bool> occupied{false};
    std::atomic<int> pendingKeyswitch{-1};

    // Stage placement targets (control thread / CC writes; audio reads).
    std::atomic<float> stageX{0.0f}, stageDepth{0.35f}, width{1.0f};

    // Mixer (control writes; audio reads and smooths).
    std::atomic<float> gainDb{0.0f};
    std::atomic<bool> mute{false}, solo{false};
    float smMix = 1.0f;

    // Live per-channel controller state (audio thread).
    float liveDynamics = -1.0f, liveExpression = -1.0f;

    // Smoothed audio-thread state.
    float smDynGain = 0.5f, smExprGain = 1.0f, smCutoffCoef = 1.0f;
    float smPanL = 1.0f, smPanR = 1.0f, smDirect = 1.0f;
    float lpL = 0.0f, lpR = 0.0f, depthLpL = 0.0f, depthLpR = 0.0f;

    float lastEarlyX = -99.0f, lastEarlyDepth = -1.0f;

    MidiEvent events[kMaxSlotEvents];

    void resetSmoothing()
    {
        lpL = lpR = depthLpL = depthLpR = 0.0f;
        liveDynamics = liveExpression = -1.0f;
        lastEarlyX = -99.0f;
        lastEarlyDepth = -1.0f;
    }
};

// ------------------------------------------------------------------ engine --

OrchestraEngine::OrchestraEngine()
{
    slots_.reserve(kNumSlots);
    for (int i = 0; i < kNumSlots; ++i)
        slots_.push_back(std::make_unique<Slot>());
}

OrchestraEngine::~OrchestraEngine() = default;

void OrchestraEngine::prepare(double sampleRate, int maxBlockFrames)
{
    sampleRate_ = sampleRate;
    maxBlock_ = maxBlockFrames;
    for (auto& slot : slots_) {
        slot->sampler.prepare(sampleRate, maxBlockFrames);
        slot->early.prepare(sampleRate);
        slot->resetSmoothing();
    }
    hall_.prepare(sampleRate);

    const size_t n = size_t(maxBlockFrames);
    for (auto* buffer : {&dryL_, &dryR_, &erTmpL_, &erTmpR_, &erAccL_, &erAccR_,
                         &sendL_, &sendR_, &tailL_, &tailR_})
        buffer->assign(n, 0.0f);

    lastHallSize_ = lastHallDecay_ = lastHallDamp_ = lastHallMod_ = -1.0f;
    lastQuality_ = -1;
    lastDnaCents_ = -1.0f;
    lastLegato_ = -1;
}

void OrchestraEngine::setInstrument(sapp::sounds::InstrumentPtr instrument, int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;
    slots_[size_t(slot)]->occupied.store(instrument != nullptr, std::memory_order_release);
    slots_[size_t(slot)]->sampler.setInstrument(std::move(instrument));
}

void OrchestraEngine::collectRetired()
{
    for (auto& slot : slots_) slot->sampler.collectRetired();
}

sapp::sounds::InstrumentPtr OrchestraEngine::currentInstrument(int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return nullptr;
    return slots_[size_t(slot)]->sampler.currentInstrument();
}

bool OrchestraEngine::slotOccupied(int slot) const
{
    return slot >= 0 && slot < kNumSlots &&
           slots_[size_t(slot)]->occupied.load(std::memory_order_acquire);
}

int OrchestraEngine::occupiedSlotCount() const
{
    int count = 0;
    for (const auto& slot : slots_)
        if (slot->occupied.load(std::memory_order_acquire)) ++count;
    return count;
}

void OrchestraEngine::selectArticulation(int index, int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;
    auto inst = slots_[size_t(slot)]->sampler.currentInstrument();
    if (!inst) return;
    const auto& arts = inst->definition.articulations;
    if (index < 0 || size_t(index) >= arts.size()) return;
    if (arts[size_t(index)].keyswitch < 0) return;
    slots_[size_t(slot)]->pendingKeyswitch.store(arts[size_t(index)].keyswitch,
                                                 std::memory_order_release);
}

void OrchestraEngine::setSlotStage(int slot, float x, float depth, float width)
{
    if (slot < 0 || slot >= kNumSlots) return;
    auto& s = *slots_[size_t(slot)];
    s.stageX.store(std::clamp(x, -1.0f, 1.0f), std::memory_order_relaxed);
    s.stageDepth.store(std::clamp(depth, 0.0f, 1.0f), std::memory_order_relaxed);
    s.width.store(std::clamp(width, 0.0f, 2.0f), std::memory_order_relaxed);
}

void OrchestraEngine::getSlotStage(int slot, float& x, float& depth, float& width) const
{
    if (slot < 0 || slot >= kNumSlots) { x = 0; depth = 0.35f; width = 1; return; }
    const auto& s = *slots_[size_t(slot)];
    x = s.stageX.load(std::memory_order_relaxed);
    depth = s.stageDepth.load(std::memory_order_relaxed);
    width = s.width.load(std::memory_order_relaxed);
}

void OrchestraEngine::setSlotMix(int slot, float gainDb, bool mute, bool solo)
{
    if (slot < 0 || slot >= kNumSlots) return;
    auto& s = *slots_[size_t(slot)];
    s.gainDb.store(std::clamp(gainDb, -60.0f, 12.0f), std::memory_order_relaxed);
    s.mute.store(mute, std::memory_order_relaxed);
    s.solo.store(solo, std::memory_order_relaxed);
}

void OrchestraEngine::getSlotMix(int slot, float& gainDb, bool& mute, bool& solo) const
{
    if (slot < 0 || slot >= kNumSlots) { gainDb = 0; mute = solo = false; return; }
    const auto& s = *slots_[size_t(slot)];
    gainDb = s.gainDb.load(std::memory_order_relaxed);
    mute = s.mute.load(std::memory_order_relaxed);
    solo = s.solo.load(std::memory_order_relaxed);
}

void OrchestraEngine::setParams(const OrchestraParams& params)
{
    const int inactive = 1 - paramIndex_.load(std::memory_order_acquire);
    paramSlots_[inactive] = params;
    paramIndex_.store(inactive, std::memory_order_release);
}

OrchestraParams OrchestraEngine::params() const
{
    return paramSlots_[paramIndex_.load(std::memory_order_acquire)];
}

void OrchestraEngine::resetSequences()
{
    for (auto& slot : slots_) slot->sampler.resetSequences();
}

void OrchestraEngine::reseed(uint32_t seed)
{
    uint32_t s = seed;
    for (auto& slot : slots_) slot->sampler.reseed(s++);
}

const sapp::sounds::PlaybackEngine& OrchestraEngine::sampler(int slot) const
{
    return slots_[size_t(std::clamp(slot, 0, kNumSlots - 1))]->sampler;
}
sapp::sounds::PlaybackEngine& OrchestraEngine::sampler(int slot)
{
    return slots_[size_t(std::clamp(slot, 0, kNumSlots - 1))]->sampler;
}

void OrchestraEngine::applyShared(const OrchestraParams& p) noexcept
{
    if (p.quality != lastQuality_) {
        lastQuality_ = p.quality;
        for (auto& slot : slots_)
            slot->sampler.setInterpolationQuality(p.quality == 0 ? 0 : 1);
    }
    const float cents = p.dnaMode == 0 ? 0.0f
                        : p.dnaAmount * (p.dnaMode == 2 ? 9.0f : 5.0f);
    if (cents != lastDnaCents_) {
        lastDnaCents_ = cents;
        for (auto& slot : slots_) slot->sampler.setRandomTuneCents(cents);
    }
    const int legato = p.legato >= 0.5f ? 1 : 0;
    if (legato != lastLegato_) {
        lastLegato_ = legato;
        for (auto& slot : slots_) slot->sampler.setLegato(legato != 0);
    }
    if (p.hallSize != lastHallSize_ || p.hallDecay != lastHallDecay_ ||
        p.hallDamping != lastHallDamp_ || p.hallModulation != lastHallMod_) {
        lastHallSize_ = p.hallSize;
        lastHallDecay_ = p.hallDecay;
        lastHallDamp_ = p.hallDamping;
        lastHallMod_ = p.hallModulation;
        hall_.setParams(p.hallSize, p.hallDecay, p.hallDamping, p.hallModulation);
    }
}

// Render one slot's sampler through its policy chain, accumulating into the
// direct bus (outL/outR), the ER accumulator, and the shared hall send.
void OrchestraEngine::processSlot(Slot& s, const OrchestraParams& p, bool anySolo,
                                  const MidiEvent* events, int eventCount,
                                  float* outL, float* outR, int frames) noexcept
{
    const int n = std::min(frames, maxBlock_);

    // Articulation injection + live CC interception (still forwarded).
    MidiEvent local[kMaxSlotEvents];
    int count = 0;
    const int ks = s.pendingKeyswitch.exchange(-1, std::memory_order_acq_rel);
    if (ks >= 0) {
        MidiEvent e;
        e.type = MidiEvent::Type::NoteOn;
        e.frame = 0;
        e.note = uint8_t(ks);
        e.value = 1;
        local[count++] = e;
        MidiEvent off = e;
        off.type = MidiEvent::Type::NoteOff;
        local[count++] = off;
    }
    for (int i = 0; i < eventCount && count < kMaxSlotEvents; ++i) {
        const auto& e = events[i];
        if (e.type == MidiEvent::Type::Controller) {
            switch (e.note) {
                case 1: s.liveDynamics = float(e.value) / 127.0f; break;
                case 11: s.liveExpression = float(e.value) / 127.0f; break;
                case 16: case 17: case 18: {
                    if (const auto* m = sapplink::findMapping(e.note)) {
                        const float v = sapplink::ccToEngineering(*m, e.value);
                        if (e.note == 16) s.stageX.store(v, std::memory_order_relaxed);
                        else if (e.note == 17) s.stageDepth.store(v, std::memory_order_relaxed);
                        else s.width.store(v, std::memory_order_relaxed);
                    }
                    break;
                }
                default: break;
            }
        }
        local[count++] = e;
    }

    const bool hasVoices = s.sampler.activeVoiceCount() > 0;
    if (!s.occupied.load(std::memory_order_acquire) && !hasVoices && count == 0)
        return;

    std::fill(dryL_.begin(), dryL_.begin() + n, 0.0f);
    std::fill(dryR_.begin(), dryR_.begin() + n, 0.0f);
    s.sampler.process(local, count, dryL_.data(), dryR_.data(), n);

    // Targets from live CCs (fall back to shared param defaults).
    const float dynamics = s.liveDynamics >= 0.0f ? s.liveDynamics : p.dynamics;
    const float expression = s.liveExpression >= 0.0f ? s.liveExpression : p.expression;
    const float dynGainTarget = dbToGain(-22.0f * (1.0f - dynamics));
    const float exprGainTarget = dbToGain(-40.0f * (1.0f - expression));
    const float bright = p.brightnessMin + (1.0f - p.brightnessMin) * dynamics;
    const float cutoffHz = 700.0f * std::pow(16000.0f / 700.0f, std::pow(bright, 0.8f));
    const float cutoffCoefTarget =
        1.0f - std::exp(-6.2831853f * cutoffHz / float(sampleRate_));

    const float stageX = s.stageX.load(std::memory_order_relaxed);
    const float depth = s.stageDepth.load(std::memory_order_relaxed);
    const float widthAmt = s.width.load(std::memory_order_relaxed);
    const float angle = (std::clamp(stageX, -1.0f, 1.0f) + 1.0f) * 0.78539816f * 0.5f + 0.39269908f;
    const float panLTarget = std::cos(angle) * 1.41421356f;
    const float panRTarget = std::sin(angle) * 1.41421356f;
    const float directTarget = dbToGain(-7.0f * depth);
    const float depthCut = 18000.0f * std::pow(0.19f, depth);
    const float depthCoef = 1.0f - std::exp(-6.2831853f * depthCut / float(sampleRate_));

    if (stageX != s.lastEarlyX || depth != s.lastEarlyDepth) {
        s.lastEarlyX = stageX;
        s.lastEarlyDepth = depth;
        s.early.setPosition(4.0f + depth * 26.0f, stageX, 0.25f + 0.6f * depth);
    }

    const float smFast = smoothCoef(sampleRate_, 12.0f);
    const float smSlow = smoothCoef(sampleRate_, 40.0f);
    const float erGain = p.earlyLevel * (0.4f + 0.9f * depth);
    const float tailSendGain = 0.55f + 0.75f * depth;

    // Mixer: mute, and solo-elsewhere, silence this slot (smoothed).
    const bool audible = !s.mute.load(std::memory_order_relaxed) &&
                         (!anySolo || s.solo.load(std::memory_order_relaxed));
    const float mixTarget = audible
                                ? dbToGain(s.gainDb.load(std::memory_order_relaxed))
                                : 0.0f;

    for (int f = 0; f < n; ++f) {
        s.smDynGain += smFast * (dynGainTarget - s.smDynGain);
        s.smExprGain += smFast * (exprGainTarget - s.smExprGain);
        s.smCutoffCoef += smSlow * (cutoffCoefTarget - s.smCutoffCoef);
        s.smPanL += smSlow * (panLTarget - s.smPanL);
        s.smPanR += smSlow * (panRTarget - s.smPanR);
        s.smDirect += smSlow * (directTarget - s.smDirect);

        float l = dryL_[size_t(f)];
        float r = dryR_[size_t(f)];

        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * widthAmt;
        l = mid + side;
        r = mid - side;

        s.lpL += s.smCutoffCoef * (l - s.lpL);
        s.lpR += s.smCutoffCoef * (r - s.lpR);
        l = s.lpL;
        r = s.lpR;

        s.smMix += smFast * (mixTarget - s.smMix);
        s.depthLpL += depthCoef * (l - s.depthLpL);
        s.depthLpR += depthCoef * (r - s.depthLpR);
        l = s.depthLpL * s.smDynGain * s.smExprGain * s.smMix;
        r = s.depthLpR * s.smDynGain * s.smExprGain * s.smMix;

        const float pl = l * s.smPanL;
        const float pr = r * s.smPanR;

        outL[f] += pl * s.smDirect;
        outR[f] += pr * s.smDirect;
        dryL_[size_t(f)] = pl;  // reuse as the placed signal for the ER feed
        dryR_[size_t(f)] = pr;
    }

    s.early.process(dryL_.data(), dryR_.data(), erTmpL_.data(), erTmpR_.data(), n);
    for (int f = 0; f < n; ++f) {
        erAccL_[size_t(f)] += erTmpL_[size_t(f)] * erGain;
        erAccR_[size_t(f)] += erTmpR_[size_t(f)] * erGain;
        sendL_[size_t(f)] += (dryL_[size_t(f)] * 0.8f + erTmpL_[size_t(f)] * 0.6f) * tailSendGain;
        sendR_[size_t(f)] += (dryR_[size_t(f)] * 0.8f + erTmpR_[size_t(f)] * 0.6f) * tailSendGain;
    }
}

void OrchestraEngine::process(const MidiEvent* events, int eventCount,
                              float* outL, float* outR, int frames) noexcept
{
    const OrchestraParams p = paramSlots_[paramIndex_.load(std::memory_order_acquire)];
    applyShared(p);

    const int n = std::min(frames, maxBlock_);
    std::fill(outL, outL + frames, 0.0f);
    std::fill(outR, outR + frames, 0.0f);
    std::fill(erAccL_.begin(), erAccL_.begin() + n, 0.0f);
    std::fill(erAccR_.begin(), erAccR_.begin() + n, 0.0f);
    std::fill(sendL_.begin(), sendL_.begin() + n, 0.0f);
    std::fill(sendR_.begin(), sendR_.begin() + n, 0.0f);

    // Routing: omni while at most one slot is occupied, strict per-channel
    // once a second instrument is loaded.
    int occupiedCount = 0, firstOccupied = 0;
    for (int i = 0; i < kNumSlots; ++i)
        if (slots_[size_t(i)]->occupied.load(std::memory_order_acquire)) {
            if (occupiedCount == 0) firstOccupied = i;
            ++occupiedCount;
        }
    const bool omni = occupiedCount <= 1;

    // Fixed per-slot event staging (no allocation).
    int slotCounts[kNumSlots] = {};
    for (int i = 0; i < eventCount; ++i) {
        const int slot = omni ? firstOccupied
                              : std::clamp(int(events[i].channel), 0, kNumSlots - 1);
        auto& s = *slots_[size_t(slot)];
        if (slotCounts[slot] < kMaxSlotEvents)
            s.events[slotCounts[slot]++] = events[i];
    }

    bool anySolo = false;
    for (const auto& slot : slots_)
        if (slot->solo.load(std::memory_order_relaxed)) anySolo = true;

    for (int i = 0; i < kNumSlots; ++i)
        processSlot(*slots_[size_t(i)], p, anySolo, slots_[size_t(i)]->events,
                    slotCounts[i], outL, outR, n);

    hall_.process(sendL_.data(), sendR_.data(), tailL_.data(), tailR_.data(), n);

    const float smSlow = smoothCoef(sampleRate_, 40.0f);
    const float masterTarget = dbToGain(p.masterGainDb);
    const bool dna = p.dnaMode != 0 && p.dnaAmount > 0.0f;
    const float driftDepth = dna ? 0.015f * p.dnaAmount : 0.0f;
    const float driftInc = float(2.0 * 3.14159265 * 0.13 / sampleRate_);
    const float noiseAmp = (p.dnaMode == 2) ? 3.0e-4f * p.dnaAmount : 0.0f;

    for (int f = 0; f < n; ++f) {
        smEarly_ += smSlow * (1.0f - smEarly_);  // ER pre-scaled per slot
        smTail_ += smSlow * (p.tailLevel - smTail_);
        smMaster_ += smSlow * (masterTarget - smMaster_);

        float gain = smMaster_;
        if (driftDepth > 0.0f) {
            dnaPhase_ += driftInc;
            if (dnaPhase_ > 6.2831853f) dnaPhase_ -= 6.2831853f;
            gain *= 1.0f + driftDepth * std::sin(dnaPhase_);
        }

        float l = (outL[f] + erAccL_[size_t(f)] + tailL_[size_t(f)] * smTail_) * gain;
        float r = (outR[f] + erAccR_[size_t(f)] + tailR_[size_t(f)] * smTail_) * gain;

        if (noiseAmp > 0.0f) {
            dnaNoise_ = dnaNoise_ * 1664525u + 1013904223u;
            const float noise = (float(dnaNoise_ >> 9) * (1.0f / 8388608.0f) - 1.0f) * noiseAmp;
            l += noise;
            r -= noise * 0.7f;
        }
        if (p.limiter) {
            l = std::tanh(l);
            r = std::tanh(r);
        }
        outL[f] = l;
        outR[f] = r;
    }
    for (int f = n; f < frames; ++f) {
        outL[f] = 0.0f;
        outR[f] = 0.0f;
    }
}

} // namespace sapp::orchestra
