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
#include "../core/SappLinkCCMap.h"
#include "../core/SfzLibrary.h"
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
    const char* name;      // CLI --param name (snake_case)
    const char* apvtsId;   // stable plugin parameter ID (= SappLink manifest id)
    float OrchestraParams::* field;
    float lo, hi, def;
    int nativeCc;          // engine-native CC (1/11), or -1
    const char* doc;
};

// Single source of truth for the float parameters an agent may set.
// MIDI CC reachability comes from the SappLink table (core/SappLinkCCMap.cpp)
// except dynamics/expression, which are engine-native on CC 1 / CC 11.
const ParamSpec kParams[] = {
    {"dynamics", "dynamics", &OrchestraParams::dynamics, 0.0f, 1.0f, 0.7f, 1,
     "Orchestral dynamics (follows MIDI CC1). Level and timbre together: pp is quiet and dark, ff is full and bright."},
    {"expression", "expression", &OrchestraParams::expression, 0.0f, 1.0f, 1.0f, 11,
     "Phrase volume (follows MIDI CC11). Level only; timbre unchanged."},
    {"stage_x", "stageX", &OrchestraParams::stageX, -1.0f, 1.0f, 0.0f, -1,
     "Stage position, -1 hard left to +1 hard right."},
    {"stage_depth", "stageDepth", &OrchestraParams::stageDepth, 0.0f, 1.0f, 0.35f, -1,
     "Distance from listener: 0 close/dry, 1 back of the hall."},
    {"width", "width", &OrchestraParams::width, 0.0f, 2.0f, 1.0f, -1,
     "Stereo width before positioning: 0 mono, 1 natural, 2 wide."},
    {"early_level", "earlyLevel", &OrchestraParams::earlyLevel, 0.0f, 1.0f, 0.35f, -1,
     "Early-reflection level (position/proximity cue)."},
    {"tail_level", "tailLevel", &OrchestraParams::tailLevel, 0.0f, 1.0f, 0.30f, -1,
     "Shared hall tail level."},
    {"hall_size", "hallSize", &OrchestraParams::hallSize, 0.2f, 1.5f, 1.0f, -1,
     "Hall size scaling."},
    {"hall_decay", "hallDecay", &OrchestraParams::hallDecay, 0.3f, 12.0f, 2.6f, -1,
     "Hall decay time in seconds (T60)."},
    {"hall_damping", "hallDamping", &OrchestraParams::hallDamping, 0.0f, 1.0f, 0.45f, -1,
     "High-frequency damping of the hall tail."},
    {"dna_amount", "dnaAmount", &OrchestraParams::dnaAmount, 0.0f, 1.0f, 0.18f, -1,
     "Analog DNA amount: humanized per-note tuning, gentle drift."},
    {"clean", "clean", &OrchestraParams::clean, 0.0f, 1.0f, 0.0f, -1,
     "Scales every modeled imperfection by (1 - clean). 0 = as designed, 1 = no modeled detune/drift/hiss. Suite-wide on CC 3."},
    {"legato", "legato", &OrchestraParams::legato, 0.0f, 1.0f, 1.0f, -1,
     "Legato level 2: overlapping single-line notes slur (attack suppressed, previous note fades). Chord-safe. >=0.5 = on."},
    {"master_gain_db", "masterGain", &OrchestraParams::masterGainDb, -24.0f, 12.0f, 0.0f, -1,
     "Master output gain in dB."},
};

// Orchestral seating templates (audience view: negative x = left).
// Values feed stageX / stageDepth; agents pick by section name.
struct Seat {
    const char* name;
    float x, depth;
};
const Seat kSeats[] = {
    {"violin1", -0.65f, 0.25f}, {"violin2", -0.30f, 0.30f},
    {"viola", 0.25f, 0.30f},    {"cello", 0.55f, 0.35f},
    {"bass", 0.75f, 0.50f},     {"flute", -0.10f, 0.50f},
    {"oboe", 0.15f, 0.50f},     {"clarinet", 0.15f, 0.60f},
    {"bassoon", -0.10f, 0.60f}, {"horn", -0.45f, 0.65f},
    {"trumpet", 0.30f, 0.70f},  {"trombone", 0.45f, 0.70f},
    {"tuba", 0.60f, 0.72f},     {"timpani", 0.10f, 0.85f},
    {"percussion", -0.30f, 0.85f}, {"harp", -0.70f, 0.60f},
    {"piano", -0.20f, 0.75f},   {"choir", 0.00f, 0.90f},
    {"solo", 0.00f, 0.20f},
};

const Seat* findSeat(const std::string& name)
{
    for (const auto& s : kSeats)
        if (name == s.name) return &s;
    return nullptr;
}

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
        w.field("id", p.apvtsId);
        w.field("min", double(p.lo));
        w.field("max", double(p.hi));
        w.field("default", double(p.def));
        // MIDI reachability (SappLink): mapped CC, or the engine-native CC.
        if (p.nativeCc >= 0) {
            w.field("cc", p.nativeCc);
            w.field("ccNative", true);
        } else {
            for (const auto& m : sapplink::mappings()) {
                if (std::string(m.paramId) == p.apvtsId) {
                    w.field("cc", m.cc);
                    w.field("ccCurve", m.curve == sapplink::Curve::Log ? "log" : "linear");
                    break;
                }
            }
        }
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

int cmdSeats()
{
    JsonWriter w;
    w.beginObject();
    w.field("ok", true);
    w.key("seats");
    w.beginArray();
    for (const auto& s : kSeats) {
        w.beginObject();
        w.field("name", s.name);
        w.field("stage_x", double(s.x));
        w.field("stage_depth", double(s.depth));
        w.endObject();
    }
    w.endArray();
    w.field("doc", "Audience view: negative stage_x = left. Use with render --seat, "
                   "or set stage_x/stage_depth directly (MIDI CC 16/17 also work).");
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return 0;
}

int cmdScan(int argc, char** argv)
{
    std::string dir;
    bool includePartials = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--all") includePartials = true;
        else dir = arg;
    }
    if (dir.empty()) {
        std::fprintf(stderr, "usage: sapporchestra scan <library-dir> [--all]\n");
        return 2;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        std::fprintf(stderr, "error: not a directory: %s\n", dir.c_str());
        return 2;
    }

    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file(ec)) continue;
        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (ext != ".sfz") continue;
        // Skip include-partials (conventionally kept in "includes/" folders)
        // unless --all: they are fragments, not playable instruments.
        if (!includePartials) {
            bool partial = false;
            for (const auto& part : it->path().parent_path())
                if (part == "includes") partial = true;
            if (partial) continue;
        }
        files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());

    sapp::sounds::SfzParser parser;
    JsonWriter w;
    w.beginObject();
    w.field("ok", true);
    w.field("root", dir);
    w.key("instruments");
    w.beginArray();
    size_t playable = 0;
    for (const auto& file : files) {
        auto parsed = parser.parseFile(file);
        const auto& def = parsed.instrument;
        if (def.regions.empty()) continue;
        ++playable;
        w.beginObject();
        w.field("path", file.string());
        w.field("name", def.name);
        w.field("category",
                fs::relative(file.parent_path(), dir, ec).string());
        w.field("regions", uint64_t(def.regions.size()));
        w.field("articulations", uint64_t(def.articulations.size()));
        w.field("keyswitches", def.keyswitchLo >= 0);
        w.field("lowKey", int(def.loKeyUsed));
        w.field("highKey", int(def.hiKeyUsed));
        w.endObject();
    }
    w.endArray();
    w.field("count", uint64_t(playable));
    w.endObject();
    std::printf("%s\n", w.str().c_str());
    return 0;
}

int cmdSfzIndex(int argc, char** argv)
{
    // The enumeration behind the plugin's `instrument` choice parameter
    // (sapptune issue #20). Prints entry -> choice-index -> normalized value
    // so a driving session can map "Sonatina .../Trumpets KS" to the exact
    // live_set_param write. --rescan rewrites <root>/.sapp-sfz-index.json;
    // a running plugin instance picks the new list up on its NEXT
    // instantiation (choice lists are fixed per instance).
    std::string root;
    bool rescan = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) root = argv[++i];
        else if (arg == "--rescan") rescan = true;
    }
    if (root.empty()) {
        const char* home = std::getenv("HOME");
        root = sapp::sfzlib::resolveRoot(std::string(home ? home : "") + "/Samples");
    }

    std::vector<sapp::sfzlib::Entry> entries;
    if (rescan) {
        entries = sapp::sfzlib::scan(root);
        sapp::sfzlib::writeIndex(root, entries);
    } else {
        entries = sapp::sfzlib::loadOrScan(root);
    }

    JsonWriter w;
    w.beginObject();
    w.field("ok", true);
    w.field("root", root);
    w.field("indexFile", root + "/" + sapp::sfzlib::kIndexFileName);
    w.field("rescanned", rescan);
    w.field("count", uint64_t(entries.size()));
    w.field("note", "choice 0 = \"(keep current)\"; entry i maps to choice i+1; "
                    "normalized = choice / count; program change: bank-select "
                    "CC0/CC32 then program p selects entry (bank*128 + p)");
    w.key("entries");
    w.beginArray();
    for (size_t i = 0; i < entries.size(); ++i) {
        w.beginObject();
        w.field("entry", uint64_t(i));
        w.field("choice", uint64_t(i + 1));
        if (!entries.empty())
            w.field("normalized", double(i + 1) / double(entries.size()));
        w.field("label", entries[i].label);
        w.field("path", entries[i].path);
        w.endObject();
    }
    w.endArray();
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
        else if (arg == "--seat") {
            const std::string name = next();
            const auto* seat = findSeat(name);
            if (seat == nullptr) {
                std::fprintf(stderr, "error: unknown seat '%s' (see: sapporchestra seats)\n",
                             name.c_str());
                return 2;
            }
            options.params.stageX = seat->x;
            options.params.stageDepth = seat->depth;
        }
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
                     "  sapporchestra seats\n"
                     "  sapporchestra scan <library-dir> [--all]\n"
                     "  sapporchestra sfz-index [--root <dir>] [--rescan]\n"
                     "  sapporchestra render   (--sfz | --diagnostic) --midi <f.mid> --out <f.wav>\n"
                     "                         [--articulation IDX] [--seat NAME] [--param NAME=VALUE ...]\n");
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
    if (cmd == "seats") return cmdSeats();
    if (cmd == "scan") return cmdScan(argc, argv);
    if (cmd == "sfz-index") return cmdSfzIndex(argc, argv);
    if (cmd == "render") return cmdRender(argc, argv);

    std::fprintf(stderr, "unknown command '%s'\n", cmd.c_str());
    return 2;
}
