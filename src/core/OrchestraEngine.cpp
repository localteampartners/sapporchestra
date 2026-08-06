#include "OrchestraEngine.h"

#include <algorithm>
#include <cmath>

namespace sapp::orchestra {

using sapp::sounds::MidiEvent;

namespace {
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }

// One-pole smoothing coefficient for ~t milliseconds.
inline float smoothCoef(double sampleRate, float ms) noexcept
{
    return 1.0f - std::exp(-1.0f / (float(sampleRate) * ms * 0.001f));
}
} // namespace

OrchestraEngine::OrchestraEngine() = default;

void OrchestraEngine::prepare(double sampleRate, int maxBlockFrames)
{
    sampleRate_ = sampleRate;
    maxBlock_ = maxBlockFrames;
    sampler_.prepare(sampleRate, maxBlockFrames);
    early_.prepare(sampleRate);
    hall_.prepare(sampleRate);

    const size_t n = size_t(maxBlockFrames);
    dryL_.assign(n, 0.0f); dryR_.assign(n, 0.0f);
    sendL_.assign(n, 0.0f); sendR_.assign(n, 0.0f);
    earlyL_.assign(n, 0.0f); earlyR_.assign(n, 0.0f);
    tailL_.assign(n, 0.0f); tailR_.assign(n, 0.0f);

    lpL_ = lpR_ = depthLpL_ = depthLpR_ = 0.0f;
    liveDynamics_ = liveExpression_ = -1.0f;
    lastHallSize_ = lastHallDecay_ = lastHallDamp_ = lastHallMod_ = -1.0f;
    lastPredelay_ = lastDepthForEarly_ = -1.0f;
    lastStageX_ = -99.0f;
    lastQuality_ = -1;
    lastDnaCents_ = -1.0f;
}

void OrchestraEngine::setInstrument(sapp::sounds::InstrumentPtr instrument)
{
    sampler_.setInstrument(std::move(instrument));
}
void OrchestraEngine::collectRetired() { sampler_.collectRetired(); }
sapp::sounds::InstrumentPtr OrchestraEngine::currentInstrument() const
{
    return sampler_.currentInstrument();
}

void OrchestraEngine::selectArticulation(int index)
{
    auto inst = sampler_.currentInstrument();
    if (!inst) return;
    const auto& arts = inst->definition.articulations;
    if (index < 0 || size_t(index) >= arts.size()) return;
    if (arts[size_t(index)].keyswitch < 0) return;
    pendingArticulationKeyswitch_.store(arts[size_t(index)].keyswitch,
                                        std::memory_order_release);
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

void OrchestraEngine::resetSequences() { sampler_.resetSequences(); }
void OrchestraEngine::reseed(uint32_t seed) { sampler_.reseed(seed); }

void OrchestraEngine::applyQuality(const OrchestraParams& p) noexcept
{
    if (p.quality != lastQuality_) {
        lastQuality_ = p.quality;
        sampler_.setInterpolationQuality(p.quality == 0 ? 0 : 1);
    }
    // Analog DNA per-note detune: "ensemble breathing" via the sampler hook.
    const float cents = p.dnaMode == 0 ? 0.0f
                        : p.dnaAmount * (p.dnaMode == 2 ? 9.0f : 5.0f);
    if (cents != lastDnaCents_) {
        lastDnaCents_ = cents;
        sampler_.setRandomTuneCents(cents);
    }
    const int legato = p.legato >= 0.5f ? 1 : 0;
    if (legato != lastLegato_) {
        lastLegato_ = legato;
        sampler_.setLegato(legato != 0);
    }
}

void OrchestraEngine::process(const MidiEvent* events, int eventCount,
                              float* outL, float* outR, int frames) noexcept
{
    const OrchestraParams p = paramSlots_[paramIndex_.load(std::memory_order_acquire)];
    applyQuality(p);

    // Live CC following: CC1/CC11 persistently override the parameter values
    // so a ridden mod-wheel phrase shapes the whole performance.
    for (int i = 0; i < eventCount; ++i) {
        if (events[i].type == MidiEvent::Type::Controller) {
            if (events[i].note == 1) liveDynamics_ = float(events[i].value) / 127.0f;
            else if (events[i].note == 11) liveExpression_ = float(events[i].value) / 127.0f;
        }
    }
    const float dynamics = liveDynamics_ >= 0.0f ? liveDynamics_ : p.dynamics;
    const float expression = liveExpression_ >= 0.0f ? liveExpression_ : p.expression;

    // Articulation change requested from the UI: inject its keyswitch press
    // ahead of this block's events.
    MidiEvent localEvents[257];
    int localCount = 0;
    const int ks = pendingArticulationKeyswitch_.exchange(-1, std::memory_order_acq_rel);
    if (ks >= 0) {
        MidiEvent e;
        e.type = MidiEvent::Type::NoteOn;
        e.frame = 0;
        e.note = uint8_t(ks);
        e.value = 1;
        localEvents[localCount++] = e;
        MidiEvent off = e;
        off.type = MidiEvent::Type::NoteOff;
        localEvents[localCount++] = off;
    }
    for (int i = 0; i < eventCount && localCount < 257; ++i)
        localEvents[localCount++] = events[i];

    // --- dry sampler render -------------------------------------------------
    const int n = std::min(frames, maxBlock_);
    std::fill(dryL_.begin(), dryL_.begin() + n, 0.0f);
    std::fill(dryR_.begin(), dryR_.begin() + n, 0.0f);
    sampler_.process(localEvents, localCount, dryL_.data(), dryR_.data(), n);

    // --- target gains -------------------------------------------------------
    // CC1 dynamics: ±level and timbre. pp ≈ -22 dB and dark; ff = 0 dB bright.
    const float dynGainTarget = dbToGain(-22.0f * (1.0f - dynamics));
    const float exprGainTarget = dbToGain(-40.0f * (1.0f - expression));
    // Timbre: LP cutoff from ~700 Hz to ~16 kHz as dynamics rise.
    const float bright = p.brightnessMin + (1.0f - p.brightnessMin) * dynamics;
    const float cutoffHz = 700.0f * std::pow(16000.0f / 700.0f, std::pow(bright, 0.8f));
    const float cutoffCoefTarget =
        1.0f - std::exp(-6.2831853f * cutoffHz / float(sampleRate_));

    // Stage: pan (equal power), depth → direct attenuation + damping + room.
    const float panNorm = std::clamp(p.stageX, -1.0f, 1.0f);
    const float angle = (panNorm + 1.0f) * 0.78539816f * 0.5f + 0.39269908f;  // gentle: ±45°→±22.5°
    const float panLTarget = std::cos(angle) * 1.41421356f;
    const float panRTarget = std::sin(angle) * 1.41421356f;
    const float depth = std::clamp(p.stageDepth, 0.0f, 1.0f);
    const float directTarget = dbToGain(-7.0f * depth);
    const float earlyTarget = p.earlyLevel * (0.4f + 0.9f * depth);
    const float tailTarget = p.tailLevel * (0.55f + 0.75f * depth);
    const float masterTarget = dbToGain(p.masterGainDb);

    // Distance damping: one-pole LP from 18 kHz (close) to ~3.4 kHz (far).
    const float depthCut = 18000.0f * std::pow(0.19f, depth);
    const float depthCoef = 1.0f - std::exp(-6.2831853f * depthCut / float(sampleRate_));

    // Room parameter updates only when changed (cheap checks, RT-safe).
    const float predelayMs = 4.0f + depth * 26.0f;
    if (p.hallSize != lastHallSize_ || p.hallDecay != lastHallDecay_ ||
        p.hallDamping != lastHallDamp_ || p.hallModulation != lastHallMod_) {
        lastHallSize_ = p.hallSize; lastHallDecay_ = p.hallDecay;
        lastHallDamp_ = p.hallDamping; lastHallMod_ = p.hallModulation;
        hall_.setParams(p.hallSize, p.hallDecay, p.hallDamping, p.hallModulation);
    }
    if (predelayMs != lastPredelay_ || p.stageX != lastStageX_ || depth != lastDepthForEarly_) {
        lastPredelay_ = predelayMs; lastStageX_ = p.stageX; lastDepthForEarly_ = depth;
        early_.setPosition(predelayMs, p.stageX, 0.25f + 0.6f * depth);
    }

    const float smFast = smoothCoef(sampleRate_, 12.0f);
    const float smSlow = smoothCoef(sampleRate_, 40.0f);

    // Analog DNA drift (slow, gentle level breathing) + vintage noise floor.
    const bool dna = p.dnaMode != 0 && p.dnaAmount > 0.0f;
    const float driftDepth = dna ? 0.015f * p.dnaAmount : 0.0f;
    const float driftInc = float(2.0 * 3.14159265 * 0.13 / sampleRate_);
    const float noiseAmp = (p.dnaMode == 2) ? 3.0e-4f * p.dnaAmount : 0.0f;

    const float widthAmt = std::clamp(p.width, 0.0f, 2.0f);

    // --- per-sample dry chain ----------------------------------------------
    for (int f = 0; f < n; ++f) {
        smDynGain_ += smFast * (dynGainTarget - smDynGain_);
        smExprGain_ += smFast * (exprGainTarget - smExprGain_);
        smCutoffCoef_ += smSlow * (cutoffCoefTarget - smCutoffCoef_);
        smPanL_ += smSlow * (panLTarget - smPanL_);
        smPanR_ += smSlow * (panRTarget - smPanR_);
        smDirect_ += smSlow * (directTarget - smDirect_);
        smEarly_ += smSlow * (earlyTarget - smEarly_);
        smTail_ += smSlow * (tailTarget - smTail_);
        smMaster_ += smSlow * (masterTarget - smMaster_);

        float l = dryL_[size_t(f)];
        float r = dryR_[size_t(f)];

        // Width (mid/side) before positioning.
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * widthAmt;
        l = mid + side;
        r = mid - side;

        // Dynamics timbre filter.
        lpL_ += smCutoffCoef_ * (l - lpL_);
        lpR_ += smCutoffCoef_ * (r - lpR_);
        l = lpL_;
        r = lpR_;

        // Distance damping.
        depthLpL_ += depthCoef * (l - depthLpL_);
        depthLpR_ += depthCoef * (r - depthLpR_);
        l = depthLpL_;
        r = depthLpR_;

        float gain = smDynGain_ * smExprGain_;
        if (driftDepth > 0.0f) {
            dnaPhase_ += driftInc;
            if (dnaPhase_ > 6.2831853f) dnaPhase_ -= 6.2831853f;
            gain *= 1.0f + driftDepth * std::sin(dnaPhase_);
        }
        l *= gain;
        r *= gain;

        // Stage pan on the placed source.
        const float pl = l * smPanL_;
        const float pr = r * smPanR_;

        dryL_[size_t(f)] = pl * smDirect_;
        dryR_[size_t(f)] = pr * smDirect_;
        sendL_[size_t(f)] = pl;
        sendR_[size_t(f)] = pr;

        if (noiseAmp > 0.0f) {
            dnaNoise_ = dnaNoise_ * 1664525u + 1013904223u;
            const float noise = (float(dnaNoise_ >> 9) * (1.0f / 8388608.0f) - 1.0f) * noiseAmp;
            dryL_[size_t(f)] += noise;
            dryR_[size_t(f)] -= noise * 0.7f;
        }
    }

    // --- room ---------------------------------------------------------------
    early_.process(sendL_.data(), sendR_.data(), earlyL_.data(), earlyR_.data(), n);
    // The hall is fed by direct sound + early reflections (coherent space).
    for (int f = 0; f < n; ++f) {
        sendL_[size_t(f)] = sendL_[size_t(f)] * 0.8f + earlyL_[size_t(f)] * 0.6f;
        sendR_[size_t(f)] = sendR_[size_t(f)] * 0.8f + earlyR_[size_t(f)] * 0.6f;
    }
    hall_.process(sendL_.data(), sendR_.data(), tailL_.data(), tailR_.data(), n);

    for (int f = 0; f < n; ++f) {
        float l = (dryL_[size_t(f)] + earlyL_[size_t(f)] * smEarly_ +
                   tailL_[size_t(f)] * smTail_) * smMaster_;
        float r = (dryR_[size_t(f)] + earlyR_[size_t(f)] * smEarly_ +
                   tailR_[size_t(f)] * smTail_) * smMaster_;
        if (p.limiter) {
            // Continuous soft saturation: ~transparent at low level, caps at ±1.
            l = std::tanh(l);
            r = std::tanh(r);
        }
        outL[f] = l;
        outR[f] = r;
    }
    for (int f = n; f < frames; ++f) { outL[f] = 0.0f; outR[f] = 0.0f; }
}

} // namespace sapp::orchestra
