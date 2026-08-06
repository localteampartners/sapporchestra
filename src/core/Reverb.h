#pragma once
// SappOrchestra room: early reflections (position/proximity) + shared hall
// tail (8-line FDN, Householder feedback, damped + gently modulated).
// Framework-independent, realtime-safe after prepare().

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sapp::orchestra {

// ------------------------------------------------------------ early taps ---
class EarlyReflections {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        const double maxSeconds = 0.25;
        buffer_.assign(size_t(sampleRate * maxSeconds) + 4, 0.0f);
        writePos_ = 0;
        lpL_ = lpR_ = 0.0f;
    }

    // predelayMs: distance/proximity. stageX in [-1,1] skews the tap pattern.
    void setPosition(float predelayMs, float stageX, float damping)
    {
        predelaySamples_ = float(predelayMs * 0.001 * sampleRate_);
        stageX_ = std::clamp(stageX, -1.0f, 1.0f);
        dampCoef_ = 1.0f - std::clamp(damping, 0.0f, 0.98f);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int frames)
    {
        static constexpr float tapMs[8] = {11.3f, 17.9f, 23.7f, 31.1f, 38.3f, 47.9f, 55.1f, 63.7f};
        static constexpr float tapGain[8] = {0.85f, 0.72f, 0.62f, 0.50f, 0.41f, 0.33f, 0.26f, 0.20f};

        const int size = int(buffer_.size());
        for (int f = 0; f < frames; ++f) {
            buffer_[size_t(writePos_)] = 0.5f * (inL[f] + inR[f]);

            float l = 0.0f, r = 0.0f;
            for (int t = 0; t < 8; ++t) {
                // Source position skews left/right arrival times slightly.
                const float skew = stageX_ * 2.3f * float(t % 2 == 0 ? 1 : -1);
                const float delayMs = tapMs[t] + skew;
                const float delay = predelaySamples_ + float(delayMs * 0.001 * sampleRate_);
                int idx = writePos_ - int(delay);
                while (idx < 0) idx += size;
                const float v = buffer_[size_t(idx)] * tapGain[t];
                if (t % 2 == 0) { l += v; r += v * 0.6f; }
                else            { r += v; l += v * 0.6f; }
            }
            // High-frequency absorption grows with distance.
            lpL_ += dampCoef_ * (l - lpL_);
            lpR_ += dampCoef_ * (r - lpR_);
            outL[f] = lpL_ * 0.5f;
            outR[f] = lpR_ * 0.5f;

            if (++writePos_ >= size) writePos_ = 0;
        }
    }

private:
    std::vector<float> buffer_;
    int writePos_ = 0;
    double sampleRate_ = 48000.0;
    float predelaySamples_ = 0.0f;
    float stageX_ = 0.0f;
    float dampCoef_ = 0.6f;
    float lpL_ = 0.0f, lpR_ = 0.0f;
};

// -------------------------------------------------------------- FDN hall ---
class HallReverb {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        // Mutually prime base delays (ms) for a dense, colorless tail.
        static constexpr float baseMs[kLines] = {29.7f, 37.1f, 41.1f, 47.3f, 53.9f, 61.7f, 71.9f, 83.3f};
        for (int i = 0; i < kLines; ++i) {
            baseSamples_[i] = float(baseMs[i] * 0.001 * sampleRate);
            const size_t cap = size_t(baseSamples_[i] * 2.2f) + 64;
            lines_[i].assign(cap, 0.0f);
            writePos_[i] = 0;
            damp_[i] = 0.0f;
            lfoPhase_[i] = float(i) * 0.785f;
        }
        inDiffL_.prepare(sampleRate, 4.7f, 0.62f);
        inDiffR_.prepare(sampleRate, 3.6f, 0.62f);
        update();
    }

    void setParams(float size, float decaySeconds, float damping, float modDepth)
    {
        size_ = std::clamp(size, 0.2f, 1.5f);
        decay_ = std::clamp(decaySeconds, 0.3f, 12.0f);
        damping_ = std::clamp(damping, 0.0f, 1.0f);
        modDepth_ = std::clamp(modDepth, 0.0f, 1.0f);
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int frames)
    {
        for (int f = 0; f < frames; ++f) {
            const float inMono = 0.35f * (inDiffL_.tick(inL[f]) + inDiffR_.tick(inR[f]));

            // Read modulated taps.
            float read[kLines];
            float sum = 0.0f;
            for (int i = 0; i < kLines; ++i) {
                lfoPhase_[i] += lfoInc_[i];
                if (lfoPhase_[i] > 6.2831853f) lfoPhase_[i] -= 6.2831853f;
                const float mod = std::sin(lfoPhase_[i]) * modSamples_;
                const float delay = delaySamples_[i] + mod;
                const int size = int(lines_[i].size());
                float pos = float(writePos_[i]) - delay;
                while (pos < 0.0f) pos += float(size);
                const int i0 = int(pos);
                const float frac = pos - float(i0);
                const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
                read[i] = lines_[i][size_t(i0)] * (1.0f - frac) + lines_[i][size_t(i1)] * frac;
                sum += read[i];
            }

            // Householder feedback: y_i = g * (read_i - (2/N) * sum) + input.
            const float k = 2.0f / float(kLines);
            for (int i = 0; i < kLines; ++i) {
                float v = feedback_[i] * (read[i] - k * sum) + inMono;
                // One-pole damping inside the loop.
                damp_[i] += dampCoef_ * (v - damp_[i]);
                v = damp_[i];
                lines_[i][size_t(writePos_[i])] = v;
                if (++writePos_[i] >= int(lines_[i].size())) writePos_[i] = 0;
            }

            outL[f] = (read[0] - read[2] + read[4] - read[6]) * 0.35f;
            outR[f] = (read[1] - read[3] + read[5] - read[7]) * 0.35f;
        }
    }

private:
    static constexpr int kLines = 8;

    struct Allpass {
        void prepare(double sampleRate, float ms, float g)
        {
            buffer.assign(size_t(ms * 0.001 * sampleRate) + 2, 0.0f);
            pos = 0;
            gain = g;
        }
        float tick(float x)
        {
            const float d = buffer[size_t(pos)];
            const float y = -gain * x + d;
            buffer[size_t(pos)] = x + gain * y;
            if (++pos >= int(buffer.size())) pos = 0;
            return y;
        }
        std::vector<float> buffer;
        int pos = 0;
        float gain = 0.6f;
    };

    void update()
    {
        for (int i = 0; i < kLines; ++i) {
            delaySamples_[i] = std::min(baseSamples_[i] * size_ * 2.0f,
                                        float(lines_[i].size()) - 8.0f);
            // Per-line gain for a uniform T60 across all line lengths.
            feedback_[i] = std::pow(10.0f, -3.0f * delaySamples_[i] /
                                                (decay_ * float(sampleRate_)));
            lfoInc_[i] = float((0.31 + 0.13 * i) * 2.0 * 3.14159265 / sampleRate_);
        }
        dampCoef_ = 1.0f - 0.85f * damping_;
        modSamples_ = 1.0f + modDepth_ * 6.0f;
    }

    double sampleRate_ = 48000.0;
    std::array<std::vector<float>, kLines> lines_;
    float baseSamples_[kLines] = {};
    float delaySamples_[kLines] = {};
    float feedback_[kLines] = {};
    float damp_[kLines] = {};
    float lfoPhase_[kLines] = {};
    float lfoInc_[kLines] = {};
    int writePos_[kLines] = {};
    Allpass inDiffL_, inDiffR_;

    float size_ = 1.0f, decay_ = 2.6f, damping_ = 0.4f, modDepth_ = 0.35f;
    float dampCoef_ = 0.66f, modSamples_ = 3.0f;
};

} // namespace sapp::orchestra
