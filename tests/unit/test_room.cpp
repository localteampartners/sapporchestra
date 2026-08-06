#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "core/Reverb.h"

using namespace sapp::orchestra;

namespace {

// Impulse through a processor; returns response buffer.
template <typename Fn>
std::vector<float> impulseResponse(Fn&& process, int frames)
{
    std::vector<float> inL(size_t(frames), 0.0f), inR(size_t(frames), 0.0f);
    std::vector<float> outL(size_t(frames), 0.0f), outR(size_t(frames), 0.0f);
    inL[0] = inR[0] = 1.0f;
    process(inL.data(), inR.data(), outL.data(), outR.data(), frames);
    return outL;
}

float rmsRange(const std::vector<float>& x, size_t a, size_t b)
{
    double sum = 0.0;
    for (size_t i = a; i < b && i < x.size(); ++i) sum += double(x[i]) * x[i];
    return float(std::sqrt(sum / double(b - a)));
}

} // namespace

TEST_CASE("early reflections arrive after predelay and decay", "[room]")
{
    EarlyReflections er;
    er.prepare(48000);
    er.setPosition(20.0f, 0.0f, 0.3f);  // 20 ms predelay

    auto ir = impulseResponse(
        [&](const float* a, const float* b, float* c, float* d, int n) { er.process(a, b, c, d, n); },
        24000);

    // Nothing before the predelay (~960 frames), energy after.
    float before = 0.0f, after = 0.0f;
    for (size_t i = 0; i < 900; ++i) before = std::max(before, std::abs(ir[i]));
    for (size_t i = 960; i < 6000; ++i) after = std::max(after, std::abs(ir[i]));
    CHECK(before < 1.0e-6f);
    CHECK(after > 0.01f);
}

TEST_CASE("hall tail is dense, decaying, and finite", "[room]")
{
    HallReverb hall;
    hall.prepare(48000);
    hall.setParams(1.0f, 2.0f, 0.4f, 0.35f);

    auto ir = impulseResponse(
        [&](const float* a, const float* b, float* c, float* d, int n) { hall.process(a, b, c, d, n); },
        96000);

    for (float v : ir) REQUIRE(std::isfinite(v));

    const float early = rmsRange(ir, 4800, 14400);    // 0.1–0.3 s
    const float mid = rmsRange(ir, 48000, 57600);     // 1.0–1.2 s
    const float late = rmsRange(ir, 86400, 96000);    // 1.8–2.0 s
    CHECK(early > 1.0e-4f);   // tail exists
    CHECK(mid < early);       // it decays
    CHECK(late < mid);        // monotonically-ish
    CHECK(late > 1.0e-7f);    // but is still alive inside T60
}

TEST_CASE("longer decay setting yields longer tail", "[room]")
{
    auto tailAt = [](float decaySeconds) {
        HallReverb hall;
        hall.prepare(48000);
        hall.setParams(1.0f, decaySeconds, 0.3f, 0.3f);
        auto ir = impulseResponse(
            [&](const float* a, const float* b, float* c, float* d, int n) { hall.process(a, b, c, d, n); },
            96000);
        return rmsRange(ir, 72000, 96000);  // energy at 1.5–2.0 s
    };
    CHECK(tailAt(6.0f) > tailAt(1.0f) * 3.0f);
}
