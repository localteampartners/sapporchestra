// sapporchestra — the SappOrchestra agent/automation CLI.
//
// This is the machine API for external software (e.g. MIDI-generation
// agents): inspect an instrument's capabilities, validate SFZ, dump the
// parameter schema, and render MIDI through the full orchestra chain.
// Every output is a single JSON document on stdout.
//
//   sapporchestra inspect  (--sfz <f.sfz> | --diagnostic) [--regions]
//   sapporchestra validate --sfz <f.sfz>
//   sapporchestra params
//   sapporchestra render   (--sfz <f.sfz> | --diagnostic) --midi <f.mid>
//                          --out <f.wav> [--sr N] [--seed N] [--tail S]
//                          [--articulation IDX] [--param NAME=VALUE ...]
//
// See docs/agent_api.md for the full contract.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sapp/sounds/DiagnosticInstrument.h>
#include <sapp/sounds/InstrumentLoader.h>
#include <sapp/sounds/MidiFile.h>
#include <sapp/sounds/WavIo.h>

#include "../core/OrchestraRender.h"
#include "Json.h"

using namespace sapp::sounds;
using namespace sapp::orchestra;
using sapptools::JsonWriter;

namespace {

const char* noteName(int note)
{
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static char buf[8];
    std::snprintf(buf, sizeof(buf), "%s%d", names[((note % 12) + 12) % 12], note / 12 - 1);
    return buf;
}

struct ParamSpec {
    const char* name;
    float OrchestraParams::* field;
    float lo, hi, def;
    const char* doc;
};

// Single source of truth for the float parameters an agent may set.
const ParamSpec kParams[] = {
    {"dynamics", &OrchestraParams::dynamics, 0.0f, 1.0f, 0.7f,
     "Orchestral dynamics (follows MIDI CC1). Level and timbre together: pp is quiet and dark, ff is full and bright."},
    {"expression", &OrchestraParams::expression, 0.0f, 1.0f, 1.0f,
     "Phrase volume (follows MIDI CC11). Level only; timbre unchanged."},
    {"stage_x", &OrchestraParams::stageX, -1.0f, 1.0f, 0.0f,
     "Stage position, -1 hard left to +1 hard right."},
    {"stage_depth", &OrchestraParams::stageDepth, 0.0f, 1.0f, 0.35f,
     "Distance from listener: 0 close/dry, 1 back of the hall."},
    {"width", &OrchestraParams::width, 0.0f, 2.0f, 1.0f,
     "Stereo width before positioning: 0 mono, 1 natural, 2 wide."},
    {"early_level", &OrchestraParams::earlyLevel, 0.0f, 1.0f, 0.35f,
     "Early-reflection level (position/proximity cue)."},
    {"tail_level", &OrchestraParams::tailLevel, 0.0f, 1.0f, 0.30f,
     "Shared hall tail level."},
    {"hall_size", &OrchestraParams::hallSize, 0.2f, 1.5f, 1.0f,
     "Hall size scaling."},
    {"hall_decay", &OrchestraParams::hallDecay, 0.3f, 12.0f, 2.6f,
     "Hall decay time in seconds (T60)."},
    {"hall_damping", &OrchestraParams::hallDamping, 0.0f, 1.0f, 0.45f,
     "High-frequency damping of the hall tail."},
    {"dna_amount", &OrchestraParams::dnaAmount, 0.0f, 1.0f, 0.18f,
     "Analog DNA amount: humanized per-note tuning, gentle drift."},
    {"master_gain_db", &OrchestraParams::masterGainDb, -24.0f, 12.0f, 0.0f,
     "Master output gain in dB."},
};

InstrumentPtr loadInstrument(const std::string& sfzPath, bool useDiagnostic,
                             std::vector<Diagnostic>& diags,
                             std::vector<std::string>& missing)
{
    if (useDiagnostic) return makeDiagnosticInstrument();
    InstrumentLoader loader;
    auto result = loader.loadSfz(sfzPath);
    diags = result.diagnostics;
    missing = result.missingSamples;
    return result.ok ? result.instrument : nullptr;
}

void writeDiagnostics(JsonWriter& w, const std::vector<Diagnostic>& diags)
{
    w.key("diagnostics");
    w.beginArray();
    for (const auto& d : diags) {
        w.beginObject();
        w.field("severity", d.severity == Severity::Error ? "error"
                          : d.severity == Severity::Warning ? "warning" : "info");
        w.field("file", d.file);
        w.field("line", d.line);
        w.field("message", d.message);
        w.endObject();
    }
    w.endArray();
}

int cmdInspect(const std::string& sfzPath, bool useDiagnostic, bool dumpRegions)
{
    std::vector<Diagnostic> diags;
    std::vector<std::string> missing;
    auto inst = loadInstrument(sfzPath, useDiagnostic, diags, missing);

    JsonWriter w;
    w.beginObject();
    if (!inst) {
        w.field("ok", false);
        writeDiagnostics(w, diags);
        w.endObject();
        std::printf("%s\n", w.str().c_str());
        return 2;
    }
    const auto& def = inst->definition;

    std::set<int> velocitySplits;
    uint16_t maxRoundRobins = 1;
    bool hasReleaseSamples = false;
    for (const auto& r : def.regions) {
        velocitySplits.insert(r.loVel);
        maxRoundRobins = std::max(maxRoundRobins, r.seqLength);
        if (r.trigger == TriggerMode::Release || r.trigger == TriggerMode::ReleaseKey)
            hasReleaseSamples = true;
    }

    w.field("ok", true);
    w.field("name", def.name);
    w.field("source", def.sourcePath.empty() ? std::string("(generated)") : def.sourcePath);
    w.field("regions", uint64_t(def.regions.size()));
    w.field("missingSamples", uint64_t(missing.size()));
    w.field("estimatedRamBytes", inst->sampleBytes());

    w.key("playableRange");
    w.beginObject();
    w.field("low", int(def.loKeyUsed));
    w.field("high", int(def.hiKeyUsed));
    w.field("lowName", noteName(def.loKeyUsed));
    w.field("highName", noteName(def.hiKeyUsed));
    w.endObject();

    // The articulation protocol an agent needs: index (for --articulation and
    // the plugin parameter) + keyswitch note (for in-stream MIDI switching).
    w.key("articulations");
    w.beginArray();
    for (size_t i = 0; i < def.articulations.size(); ++i) {
        const auto& a = def.articulations[i];
        w.beginObject();
        w.field("index", uint64_t(i));
        w.field("name", a.name);
        w.field("keyswitch", a.keyswitch);
        if (a.keyswitch >= 0) w.field("keyswitchName", noteName(a.keyswitch));
        w.field("regions", uint64_t(a.regionCount));
        w.field("default", a.isDefault);
        w.endObject();
    }
    w.endArray();

    w.key("capabilities");
    w.beginObject();
    w.field("velocityLayers", uint64_t(velocitySplits.size()));
    w.field("roundRobins", int(maxRoundRobins));
    w.field("releaseSamples", hasReleaseSamples);
    w.field("keyswitches", def.keyswitchLo >= 0);
    w.endObject();

    // Controller conventions the orchestra engine responds to.
    w.key("controllers");
    w.beginArray();
    {
        w.beginObject();
        w.field("cc", 1);
        w.field("role", "dynamics");
        w.field("doc", "Level + timbre. Ride it through phrases like an orchestral mod-wheel.");
        w.endObject();
        w.beginObject();
        w.field("cc", 11);
        w.field("role", "expression");
        w.field("doc", "Phrase volume on top of dynamics.");
        w.endObject();
        w.beginObject();
        w.field("cc", 64);
        w.field("role", "sustain");
        w.field("doc", "Sustain pedal: holds notes, defers release samples.");
        w.endObject();
    }
    w.endArray();

    if (dumpRegions) {
        w.key("regionDetails");
        w.beginArray();
        for (const auto& r : def.regions) {
            w.beginObject();
            w.field("sample", r.samplePath);
            w.field("loKey", int(r.loKey));
            w.field("hiKey", int(r.hiKey));
            w.field("rootKey", int(r.rootKey));
            w.field("loVel", int(r.loVel));
            w.field("hiVel", int(r.hiVel));
            w.field("keyswitch", r.swLast);
            w.field("seqPosition", int(r.seqPosition));
            w.field("seqLength", int(r.seqLength));
            w.field("missing", r.sample == kInvalidSample);
            w.endObject();
        }
        w.endArray();
    }

    writeDiagnostics(w, diags);
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return missing.empty() ? 0 : 1;
}

int cmdValidate(const std::string& sfzPath)
{
    std::vector<Diagnostic> diags;
    std::vector<std::string> missing;
    auto inst = loadInstrument(sfzPath, false, diags, missing);

    int errors = 0, warnings = 0;
    for (const auto& d : diags) {
        if (d.severity == Severity::Error) ++errors;
        else if (d.severity == Severity::Warning) ++warnings;
    }

    JsonWriter w;
    w.beginObject();
    w.field("ok", inst != nullptr);
    w.field("file", sfzPath);
    w.field("errors", errors);
    w.field("warnings", warnings);
    w.field("missingSamples", uint64_t(missing.size()));
    if (inst) {
        w.field("regions", uint64_t(inst->definition.regions.size()));
        w.key("unsupportedOpcodes");
        w.beginArray();
        for (const auto& o : inst->definition.unsupportedOpcodes) w.value(o);
        w.endArray();
    }
    writeDiagnostics(w, diags);
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return inst == nullptr ? 2 : (warnings > 0 || !missing.empty() ? 1 : 0);
}

int cmdParams()
{
    JsonWriter w;
    w.beginObject();
    w.field("ok", true);
    w.field("product", "SappOrchestra");
    w.key("params");
    w.beginArray();
    for (const auto& p : kParams) {
        w.beginObject();
        w.field("name", p.name);
        w.field("min", double(p.lo));
        w.field("max", double(p.hi));
        w.field("default", double(p.def));
        w.field("doc", p.doc);
        w.endObject();
    }
    w.endArray();
    w.key("enums");
    w.beginObject();
    w.key("dna_mode");
    w.beginArray();
    w.value("clean");
    w.value("cohesive");
    w.value("vintage");
    w.endArray();
    w.key("quality");
    w.beginArray();
    w.value("draft");
    w.value("normal");
    w.endArray();
    w.endObject();
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return 0;
}

int cmdRender(int argc, char** argv)
{
    std::string sfzPath, midiPath, outPath;
    bool useDiagnostic = false;
    int articulation = -1;
    OrchestraRenderOptions options;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--sfz") sfzPath = next();
        else if (arg == "--midi") midiPath = next();
        else if (arg == "--out") outPath = next();
        else if (arg == "--diagnostic") useDiagnostic = true;
        else if (arg == "--sr") options.sampleRate = std::atof(next().c_str());
        else if (arg == "--seed") options.seed = uint32_t(std::strtoul(next().c_str(), nullptr, 10));
        else if (arg == "--tail") options.tailSeconds = std::atof(next().c_str());
        else if (arg == "--articulation") articulation = std::atoi(next().c_str());
        else if (arg == "--param") {
            const std::string kv = next();
            const size_t eq = kv.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "error: --param expects NAME=VALUE, got '%s'\n", kv.c_str());
                return 2;
            }
            const std::string name = kv.substr(0, eq);
            const float value = float(std::atof(kv.c_str() + eq + 1));
            bool found = false;
            for (const auto& p : kParams) {
                if (name == p.name) {
                    options.params.*(p.field) = std::clamp(value, p.lo, p.hi);
                    found = true;
                    break;
                }
            }
            if (name == "dna_mode") { options.params.dnaMode = int(value); found = true; }
            if (name == "quality") { options.params.quality = int(value); found = true; }
            if (!found) {
                std::fprintf(stderr, "error: unknown param '%s' (see: sapporchestra params)\n",
                             name.c_str());
                return 2;
            }
        }
    }

    if ((sfzPath.empty() && !useDiagnostic) || midiPath.empty() || outPath.empty()) {
        std::fprintf(stderr, "usage: sapporchestra render (--sfz <f.sfz> | --diagnostic) "
                             "--midi <f.mid> --out <f.wav> [--sr N] [--seed N] [--tail S] "
                             "[--articulation IDX] [--param NAME=VALUE ...]\n");
        return 2;
    }

    std::vector<Diagnostic> diags;
    std::vector<std::string> missing;
    auto inst = loadInstrument(sfzPath, useDiagnostic, diags, missing);
    if (!inst) {
        JsonWriter w;
        w.beginObject();
        w.field("ok", false);
        w.field("error", "failed to load instrument");
        writeDiagnostics(w, diags);
        w.endObject();
        std::printf("%s\n", w.str().c_str());
        return 2;
    }

    auto midi = readMidiFile(midiPath);
    if (!midi.ok) {
        std::fprintf(stderr, "error: %s: %s\n", midiPath.c_str(), midi.error.c_str());
        return 2;
    }

    auto rendered = renderOrchestra(inst, midi.events, options, articulation);
    if (rendered.left.empty() ||
        !writeWavFile(outPath, rendered.left.data(), rendered.right.data(),
                      rendered.left.size(), uint32_t(options.sampleRate), true)) {
        std::fprintf(stderr, "error: render/write failed\n");
        return 2;
    }

    JsonWriter w;
    w.beginObject();
    w.field("ok", true);
    w.field("out", outPath);
    w.field("sampleRate", options.sampleRate);
    w.field("frames", uint64_t(rendered.left.size()));
    w.field("durationSeconds", double(rendered.left.size()) / options.sampleRate);
    w.field("peak", double(rendered.peak));
    w.field("rms", double(rendered.rms));
    w.field("midiEvents", uint64_t(midi.events.size()));
    w.field("seed", uint64_t(options.seed));
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr,
                     "sapporchestra — SappOrchestra agent CLI\n"
                     "  sapporchestra inspect  (--sfz <f.sfz> | --diagnostic) [--regions]\n"
                     "  sapporchestra validate --sfz <f.sfz>\n"
                     "  sapporchestra params\n"
                     "  sapporchestra render   (--sfz | --diagnostic) --midi <f.mid> --out <f.wav>\n"
                     "                         [--articulation IDX] [--param NAME=VALUE ...]\n");
        return 2;
    }
    const std::string cmd = argv[1];
    std::string sfzPath;
    bool useDiagnostic = false, dumpRegions = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sfz" && i + 1 < argc) sfzPath = argv[++i];
        else if (arg == "--diagnostic") useDiagnostic = true;
        else if (arg == "--regions") dumpRegions = true;
    }

    if (cmd == "inspect") return cmdInspect(sfzPath, useDiagnostic, dumpRegions);
    if (cmd == "validate") {
        if (sfzPath.empty()) { std::fprintf(stderr, "validate requires --sfz\n"); return 2; }
        return cmdValidate(sfzPath);
    }
    if (cmd == "params") return cmdParams();
    if (cmd == "render") return cmdRender(argc, argv);

    std::fprintf(stderr, "unknown command '%s'\n", cmd.c_str());
    return 2;
}
