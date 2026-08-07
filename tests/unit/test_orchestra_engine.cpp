#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "core/OrchestraEngine.h"

using namespace sapp::orchestra;
using sapp::sounds::MidiEvent;
using Catch::Approx;

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
    engine.setSlotStage(0, params.stageX, params.stageDepth, params.width);
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

// ----------------------------------------------------------- multitimbral ---

namespace {

std::shared_ptr<sapp::sounds::LoadedInstrument> sineInstrument(double freq)
{
    auto inst = std::make_shared<sapp::sounds::LoadedInstrument>();
    inst->definition.name = "sine";
    sapp::sounds::SampleData s;
    s.sampleRate = 48000;
    s.channels = 1;
    s.frames = 48000;
    s.data.assign(1, std::vector<float>(48000, 0.0f));
    for (size_t i = 0; i < 48000; ++i)
        s.data[0][i] = 0.5f * float(std::sin(2.0 * 3.14159265358979 * freq * double(i) / 48000.0));
    inst->samples.push_back(std::move(s));
    sapp::sounds::RegionDefinition r;
    r.sample = 0;
    r.samplePath = "gen";
    r.rootKey = 69;
    r.loKey = 0;
    r.hiKey = 127;
    r.ampeg.release = 0.01f;
    inst->definition.regions.push_back(r);
    return inst;
}

MidiEvent onCh(uint32_t frame, uint8_t note, uint8_t vel, uint8_t channel)
{
    MidiEvent e = noteOn(frame, note, vel);
    e.channel = channel;
    return e;
}

MidiEvent ccCh(uint32_t frame, uint8_t num, uint8_t value, uint8_t channel)
{
    MidiEvent e = cc(frame, num, value);
    e.channel = channel;
    return e;
}

double dominantFreq(const std::vector<float>& x, size_t a, size_t b)
{
    int crossings = 0;
    for (size_t i = a + 1; i < b && i < x.size(); ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f) ++crossings;
    return crossings / (double(b - a) / 48000.0);
}

} // namespace

TEST_CASE("multitimbral: channels route to their own slots", "[rack]")
{
    OrchestraParams dry;
    dry.tailLevel = 0.0f;
    dry.earlyLevel = 0.0f;
    dry.dnaMode = 0;
    OrchestraEngine engine;
    engine.prepare(48000, 512);
    engine.setParams(dry);
    engine.setInstrument(sineInstrument(300.0), 0);
    engine.setInstrument(sineInstrument(600.0), 1);

    // Channel 1 (slot 0) only → 300 Hz.
    auto a = run(engine, {onCh(0, 69, 100, 0)}, 24000);
    CHECK(dominantFreq(a.left, 4000, 20000) == Approx(300.0).margin(20.0));

    // Fresh engine: channel 2 (slot 1) only → 600 Hz.
    OrchestraEngine engine2;
    engine2.prepare(48000, 512);
    engine2.setParams(dry);
    engine2.setInstrument(sineInstrument(300.0), 0);
    engine2.setInstrument(sineInstrument(600.0), 1);
    auto b = run(engine2, {onCh(0, 69, 100, 1)}, 24000);
    CHECK(dominantFreq(b.left, 4000, 20000) == Approx(600.0).margin(30.0));
}

TEST_CASE("multitimbral: omni while only one slot is occupied", "[rack]")
{
    OrchestraParams dry;
    dry.tailLevel = 0.0f;
    dry.earlyLevel = 0.0f;
    OrchestraEngine engine;
    engine.prepare(48000, 512);
    engine.setParams(dry);
    engine.setInstrument(sineInstrument(300.0), 0);

    // A note on channel 6 still reaches the single loaded instrument.
    auto out = run(engine, {onCh(0, 69, 100, 5)}, 12000);
    CHECK(out.peak > 0.05f);
}

TEST_CASE("multitimbral: per-channel CC1 dynamics and per-slot stage", "[rack]")
{
    OrchestraParams dry;
    dry.tailLevel = 0.0f;
    dry.earlyLevel = 0.0f;
    dry.dnaMode = 0;
    OrchestraEngine engine;
    engine.prepare(48000, 512);
    engine.setParams(dry);
    engine.setInstrument(sineInstrument(300.0), 0);
    engine.setInstrument(sineInstrument(600.0), 1);
    engine.setSlotStage(0, -1.0f, 0.0f, 1.0f);  // violin seat: hard left
    engine.setSlotStage(1, 1.0f, 0.0f, 1.0f);   // cello seat: hard right

    // Slot 0 quiet (CC1 low on ch1), slot 1 loud (CC1 high on ch2).
    auto out = run(engine,
                   {ccCh(0, 1, 10, 0), ccCh(0, 1, 127, 1),
                    onCh(10, 69, 100, 0), onCh(10, 69, 100, 1)},
                   48000);

    double energyL = 0, energyR = 0;
    for (size_t i = 24000; i < out.left.size(); ++i) {
        energyL += double(out.left[i]) * out.left[i];
        energyR += double(out.right[i]) * out.right[i];
    }
    // Right (loud slot 1) must dominate left (quiet slot 0) by far more than
    // the pan law alone would explain.
    CHECK(energyR > energyL * 4.0);
}
