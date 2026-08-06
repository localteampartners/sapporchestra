#include <catch2/catch_test_macros.hpp>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "core/OrchestraRender.h"

using namespace sapp::orchestra;
using sapp::sounds::TimedMidiEvent;

TEST_CASE("orchestra offline render is deterministic and audible", "[render]")
{
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.5f, 3});

    std::vector<TimedMidiEvent> song;
    // CC1 swell while a chord sustains.
    for (int i = 0; i <= 10; ++i)
        song.push_back({0.15 * i, 0xB0, 0, 1, uint8_t(20 + i * 10), 0});
    for (uint8_t k : {48, 55, 64})
        song.push_back({0.05, 0x90, 0, k, 90, 0});
    for (uint8_t k : {48, 55, 64})
        song.push_back({2.0, 0x80, 0, k, 0, 0});

    OrchestraRenderOptions options;
    options.tailSeconds = 2.0;
    options.params.dnaAmount = 0.3f;

    auto a = renderOrchestra(inst, song, options);
    auto b = renderOrchestra(inst, song, options);

    REQUIRE(a.left.size() == b.left.size());
    for (size_t i = 0; i < a.left.size(); i += 131)
        REQUIRE(a.left[i] == b.left[i]);
    CHECK(a.peak > 0.02f);
    CHECK(a.peak <= 1.0f);
}

TEST_CASE("articulation index selects staccato for the whole render", "[render]")
{
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.4f, 3});

    std::vector<TimedMidiEvent> song;
    song.push_back({0.05, 0x90, 0, 60, 110, 0});
    song.push_back({3.0, 0x80, 0, 60, 0, 0});

    OrchestraRenderOptions options;
    options.tailSeconds = 1.0;
    options.params.tailLevel = 0.0f;
    options.params.earlyLevel = 0.0f;

    // Sustain articulation: still sounding at 2.5 s.
    auto sustain = renderOrchestra(inst, song, options, 0);
    // Staccato articulation: sample ends quickly.
    auto staccato = renderOrchestra(inst, song, options, 1);

    auto rmsAt = [](const std::vector<float>& x, size_t a, size_t b) {
        double sum = 0.0;
        for (size_t i = a; i < b && i < x.size(); ++i) sum += double(x[i]) * x[i];
        return std::sqrt(sum / double(b - a));
    };
    const double sustainLate = rmsAt(sustain.left, 100000, 120000);
    const double staccatoLate = rmsAt(staccato.left, 100000, 120000);
    CHECK(sustainLate > staccatoLate * 5.0);
}
