#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "core/OrchestraEngine.h"

using namespace sapp::orchestra;
using sapp::sounds::MidiEvent;

namespace {

MidiEvent noteOn(uint32_t frame, uint8_t note, uint8_t vel)
{
    MidiEvent e;
    e.type = MidiEvent::Type::NoteOn;
    e.frame = frame;
    e.note = note;
    e.value = vel;
    return e;
}

MidiEvent cc(uint32_t frame, uint8_t num, uint8_t value)
{
    MidiEvent e;
    e.type = MidiEvent::Type::Controller;
    e.frame = frame;
    e.note = num;
    e.value = value;
    return e;
}

struct Rendered {
    std::vector<float> left, right;
    float peak = 0.0f;
    float rms = 0.0f;
};

Rendered run(OrchestraEngine& engine, std::vector<MidiEvent> events, int totalFrames,
             int block = 512)
{
    Rendered out;
    out.left.assign(size_t(totalFrames), 0.0f);
    out.right.assign(size_t(totalFrames), 0.0f);
    size_t next = 0;
    for (int start = 0; start < totalFrames; start += block) {
        const int frames = std::min(block, totalFrames - start);
        std::vector<MidiEvent> blockEvents;
        while (next < events.size() && events[next].frame < uint32_t(start + frames)) {
            MidiEvent e = events[next];
            e.frame = e.frame >= uint32_t(start) ? e.frame - uint32_t(start) : 0;
            blockEvents.push_back(e);
            ++next;
        }
        engine.process(blockEvents.data(), int(blockEvents.size()),
                       out.left.data() + start, out.right.data() + start, frames);
    }
    double sumSq = 0.0;
    for (size_t i = 0; i < out.left.size(); ++i) {
        out.peak = std::max({out.peak, std::abs(out.left[i]), std::abs(out.right[i])});
        sumSq += double(out.left[i]) * out.left[i] + double(out.right[i]) * out.right[i];
    }
    out.rms = float(std::sqrt(sumSq / double(out.left.size() * 2)));
    return out;
}

OrchestraEngine& freshEngine(OrchestraEngine& engine, OrchestraParams params = {})
{
    engine.prepare(48000, 512);
    engine.setParams(params);
    engine.setInstrument(sapp::sounds::makeDiagnosticInstrument({48000, 1.2f, 0.5f, 11}));
    return engine;
}

} // namespace

TEST_CASE("engine produces sound with room tail", "[orchestra]")
{
    OrchestraEngine engine;
    freshEngine(engine);
    auto out = run(engine, {noteOn(0, 60, 100)}, 48000);
    CHECK(out.peak > 0.02f);
    // Room: signal persists in the second half (note still held + tail).
    float lateRms = 0.0f;
    for (size_t i = 24000; i < out.left.size(); ++i) lateRms += std::abs(out.left[i]);
    CHECK(lateRms / 24000.0f > 1.0e-4f);
}

TEST_CASE("CC1 dynamics changes level and brightness", "[orchestra]")
{
    OrchestraEngine a, b;
    freshEngine(a);
    freshEngine(b);

    auto quiet = run(a, {cc(0, 1, 8), noteOn(10, 60, 100)}, 48000);
    auto loud = run(b, {cc(0, 1, 127), noteOn(10, 60, 100)}, 48000);
    CHECK(loud.rms > quiet.rms * 2.0f);  // pp is ~22 dB below ff

    // Brightness: high-frequency energy ratio must rise with dynamics.
    auto hfRatio = [](const std::vector<float>& x) {
        double hf = 0.0, total = 0.0;
        float prev = 0.0f;
        for (float v : x) {
            const float d = v - prev;  // crude first-difference HP
            hf += double(d) * d;
            total += double(v) * v;
            prev = v;
        }
        return total > 0.0 ? hf / total : 0.0;
    };
    CHECK(hfRatio(loud.left) > hfRatio(quiet.left) * 1.15);
}

TEST_CASE("CC11 expression scales level without changing timbre mapping", "[orchestra]")
{
    OrchestraEngine a, b;
    freshEngine(a);
    freshEngine(b);
    auto full = run(a, {cc(0, 11, 127), noteOn(10, 60, 100)}, 24000);
    auto pulled = run(b, {cc(0, 11, 40), noteOn(10, 60, 100)}, 24000);
    CHECK(full.rms > pulled.rms * 3.0f);
}

TEST_CASE("stage position pans the source", "[orchestra]")
{
    OrchestraParams left;
    left.stageX = -1.0f;
    left.tailLevel = 0.0f;
    left.earlyLevel = 0.0f;
    OrchestraEngine a;
    freshEngine(a, left);
    auto outLeft = run(a, {noteOn(0, 60, 100)}, 24000);

    float energyL = 0.0f, energyR = 0.0f;
    for (size_t i = 0; i < outLeft.left.size(); ++i) {
        energyL += outLeft.left[i] * outLeft.left[i];
        energyR += outLeft.right[i] * outLeft.right[i];
    }
    CHECK(energyL > energyR * 1.5f);
}

TEST_CASE("stage depth attenuates and darkens", "[orchestra]")
{
    OrchestraParams close;
    close.stageDepth = 0.0f;
    close.tailLevel = 0.0f;
    close.earlyLevel = 0.0f;
    OrchestraParams far = close;
    far.stageDepth = 1.0f;

    OrchestraEngine a, b;
    freshEngine(a, close);
    freshEngine(b, far);
    auto closeOut = run(a, {noteOn(0, 60, 100)}, 24000);
    auto farOut = run(b, {noteOn(0, 60, 100)}, 24000);
    CHECK(closeOut.rms > farOut.rms * 1.4f);
}

TEST_CASE("articulation selection injects the keyswitch", "[orchestra]")
{
    OrchestraEngine engine;
    freshEngine(engine);
    run(engine, {}, 512);  // adopt instrument

    engine.selectArticulation(2);  // Pizzicato (keyswitch 14)
    run(engine, {noteOn(0, 60, 100)}, 512);

    sapp::sounds::DiagnosticSnapshot snap;
    REQUIRE(engine.sampler().diagnostics().read(snap));
    CHECK(snap.activeKeyswitch == 14);
}

TEST_CASE("output is always finite, limiter caps extremes", "[orchestra]")
{
    OrchestraParams hot;
    hot.masterGainDb = 12.0f;
    OrchestraEngine engine;
    freshEngine(engine, hot);

    std::vector<MidiEvent> wall;
    for (int i = 0; i < 24; ++i) wall.push_back(noteOn(uint32_t(i * 10), uint8_t(36 + i * 2), 127));
    auto out = run(engine, wall, 48000);
    for (float v : out.left) {
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) <= 1.0f);
    }
}
