#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "core/OrchestraRender.h"
#include "core/SappLinkCCMap.h"

// The suite-wide `clean` control (sapptune #30, CC 3). It scales EVERY
// modeled-imperfection source by (1 - clean): at 0 the engine behaves exactly
// as it always has (backwards compatible), at 1 there is no modeled noise,
// drift or detune left. In SappOrchestra every such source is driven by
// Analog DNA, so the contract lives in effectiveDnaAmount().

using namespace sapp::orchestra;
using sapp::sounds::TimedMidiEvent;

TEST_CASE("clean scales the imperfection amount, not the signal", "[clean]")
{
    OrchestraParams p;
    p.dnaAmount = 0.8f;

    p.clean = 0.0f;
    REQUIRE(effectiveDnaAmount(p) == 0.8f);       // default: unchanged
    p.clean = 0.5f;
    REQUIRE(std::abs(effectiveDnaAmount(p) - 0.4f) < 1e-6f);
    p.clean = 1.0f;
    REQUIRE(effectiveDnaAmount(p) == 0.0f);       // nothing modeled left

    // Out-of-range values clamp rather than invert the effect.
    p.clean = 2.5f;
    REQUIRE(effectiveDnaAmount(p) == 0.0f);
    p.clean = -1.0f;
    REQUIRE(effectiveDnaAmount(p) == 0.8f);
}

TEST_CASE("clean defaults to 0 — the historical behavior", "[clean]")
{
    const OrchestraParams fresh;
    REQUIRE(fresh.clean == 0.0f);
    REQUIRE(effectiveDnaAmount(fresh) == fresh.dnaAmount);
    REQUIRE(fresh.dnaAmount == 0.18f);
}

TEST_CASE("CC 3 is reserved for clean and reaches the parameter", "[clean][sapplink]")
{
    const auto* mapping = sapplink::findMapping(3);
    REQUIRE(mapping != nullptr);
    REQUIRE(std::string(mapping->paramId) == "clean");
    REQUIRE(mapping->lo == 0.0f);
    REQUIRE(mapping->hi == 1.0f);

    OrchestraParams p;
    REQUIRE(sapplink::applyCcToParams(p, 3, 127));
    REQUIRE(std::abs(p.clean - 1.0f) < 1e-5f);
    REQUIRE(sapplink::applyCcToParams(p, 3, 0));
    REQUIRE(p.clean == 0.0f);
}

TEST_CASE("clean=1 silences the vintage hiss floor", "[clean][render]")
{
    // Vintage DNA adds a noise floor. With no notes at all, the only thing
    // that can come out of the render is that modeled noise — so this measures
    // the imperfection directly.
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.4f, 3});
    const std::vector<TimedMidiEvent> silence;

    auto noiseFloor = [&](float clean) {
        OrchestraRenderOptions options;
        options.tailSeconds = 1.0;
        options.params.dnaMode = 2;        // vintage
        options.params.dnaAmount = 1.0f;
        options.params.clean = clean;
        options.params.limiter = false;
        return double(renderOrchestra(inst, silence, options).rms);
    };

    const double dirty = noiseFloor(0.0f);
    const double half = noiseFloor(0.5f);
    const double clean = noiseFloor(1.0f);

    CHECK(dirty > 0.0);
    CHECK(clean == 0.0);               // nothing modeled is left
    CHECK(half < dirty);
    CHECK(half > clean);
}

TEST_CASE("clean=0 renders bit-identically to a build without clean", "[clean][render]")
{
    // Backwards compatibility is the whole promise of the default: a session
    // saved before `clean` existed restores with clean = 0 and must sound the
    // same. `clean` is the only difference between these two renders.
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.4f, 3});
    std::vector<TimedMidiEvent> song;
    song.push_back({0.05, 0x90, 0, 60, 100, 0});
    song.push_back({1.2, 0x80, 0, 60, 0, 0});

    OrchestraRenderOptions options;
    options.tailSeconds = 0.5;
    options.params.dnaMode = 2;
    options.params.dnaAmount = 0.6f;

    options.params.clean = 0.0f;
    const auto withClean = renderOrchestra(inst, song, options);

    OrchestraRenderOptions legacy = options;   // struct default is clean = 0
    legacy.params.clean = OrchestraParams{}.clean;
    const auto asBefore = renderOrchestra(inst, song, legacy);

    REQUIRE(withClean.left.size() == asBefore.left.size());
    for (size_t i = 0; i < withClean.left.size(); i += 97)
        REQUIRE(withClean.left[i] == asBefore.left[i]);

    // And clean = 1 really does change the sound (otherwise the test above
    // would pass on a control that does nothing).
    options.params.clean = 1.0f;
    const auto cleaned = renderOrchestra(inst, song, options);
    bool differs = false;
    for (size_t i = 0; i < cleaned.left.size() && !differs; ++i)
        differs = cleaned.left[i] != asBefore.left[i];
    REQUIRE(differs);
}
