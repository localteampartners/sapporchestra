#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "core/OrchestraRender.h"
#include "core/SappLinkCCMap.h"

// The vendored manifest at tests/data/sapplink-manifest.json mirrors the
// SOURCE OF TRUTH at ~/apps/sapptune/sapplink/manifests/sapporchestra.json.
// If sapptune's manifest changes, update the vendored copy AND the table in
// src/core/SappLinkCCMap.cpp together — this test makes silent drift fail CI.

using namespace sapp::orchestra;
using namespace sapp::orchestra::sapplink;

namespace {

struct ManifestRow {
    int cc = -1;
    std::string id, curve;
    float lo = 0, hi = 0;
};

// Minimal extractor for the known manifest shape (no JSON dependency in the
// core test target): parses each object of the "parameters" array.
std::vector<ManifestRow> loadManifest(const std::string& path)
{
    std::ifstream file(path);
    REQUIRE(file.good());
    std::stringstream ss;
    ss << file.rdbuf();
    const std::string text = ss.str();

    auto grabString = [](const std::string& obj, const std::string& key) {
        const auto k = obj.find("\"" + key + "\"");
        if (k == std::string::npos) return std::string();
        const auto q1 = obj.find('"', obj.find(':', k));
        const auto q2 = obj.find('"', q1 + 1);
        return obj.substr(q1 + 1, q2 - q1 - 1);
    };

    std::vector<ManifestRow> rows;
    size_t pos = text.find("\"parameters\"");
    REQUIRE(pos != std::string::npos);
    while ((pos = text.find("{ \"id\"", pos)) != std::string::npos) {
        const auto end = text.find('}', pos);
        const std::string obj = text.substr(pos, end - pos);
        ManifestRow row;
        row.id = grabString(obj, "id");
        row.curve = grabString(obj, "curve");
        row.cc = std::stoi(obj.substr(obj.find(':', obj.find("\"cc\"")) + 1));
        const auto rangeStart = obj.find('[', obj.find("\"range\""));
        const auto comma = obj.find(',', rangeStart);
        row.lo = std::stof(obj.substr(rangeStart + 1, comma - rangeStart - 1));
        row.hi = std::stof(obj.substr(comma + 1, obj.find(']', comma) - comma - 1));
        rows.push_back(row);
        pos = end;
    }
    return rows;
}

} // namespace

TEST_CASE("SappLink table matches the vendored manifest exactly", "[sapplink]")
{
    const auto rows = loadManifest(std::string(SAPPORCH_TEST_DATA_DIR) + "/sapplink-manifest.json");
    REQUIRE(rows.size() == size_t(kNumMappings));

    for (const auto& row : rows) {
        INFO("cc " << row.cc << " id " << row.id);
        const auto* mapping = findMapping(row.cc);
        REQUIRE(mapping != nullptr);
        REQUIRE(std::string(mapping->paramId) == row.id);
        REQUIRE(mapping->lo == row.lo);
        REQUIRE(mapping->hi == row.hi);
        REQUIRE(std::string(mapping->curve == Curve::Log ? "log" : "linear") == row.curve);
    }

    // No duplicate CC assignments in the table.
    for (const auto& a : mappings())
        REQUIRE(findMapping(a.cc) == &a);
}

TEST_CASE("reserved controllers stay engine-native", "[sapplink]")
{
    // CC 1 dynamics, CC 11 expression, CC 64 sustain: never in the mapping.
    REQUIRE(findMapping(1) == nullptr);
    REQUIRE(findMapping(11) == nullptr);
    REQUIRE(findMapping(64) == nullptr);
}

TEST_CASE("CC curves interpolate correctly and monotonically", "[sapplink]")
{
    const auto* decay = findMapping(15);  // hallDecay, log 0.3..12
    REQUIRE(decay != nullptr);
    REQUIRE(std::abs(ccToEngineering(*decay, 0) - 0.3f) < 1e-4f);
    REQUIRE(std::abs(ccToEngineering(*decay, 127) - 12.0f) < 1e-3f);
    const float mid = ccToEngineering(*decay, 64);  // ≈ geometric mean ~1.9 s
    REQUIRE(mid > 1.5f);
    REQUIRE(mid < 2.4f);

    const auto* stage = findMapping(16);  // stageX, linear -1..1
    REQUIRE(stage != nullptr);
    REQUIRE(std::abs(ccToEngineering(*stage, 0) - (-1.0f)) < 1e-5f);
    REQUIRE(std::abs(ccToEngineering(*stage, 127) - 1.0f) < 1e-5f);

    for (const auto& mapping : mappings()) {
        float previous = ccToEngineering(mapping, 0);
        for (int v = 1; v <= 127; ++v) {
            const float value = ccToEngineering(mapping, v);
            REQUIRE(std::isfinite(value));
            REQUIRE(value >= previous - 1e-6f);
            previous = value;
        }
    }
}

TEST_CASE("applyCcToParams writes the mapped field and ignores others", "[sapplink]")
{
    OrchestraParams params;
    REQUIRE(applyCcToParams(params, 16, 127));
    REQUIRE(std::abs(params.stageX - 1.0f) < 1e-5f);
    REQUIRE(applyCcToParams(params, 7, 0));
    REQUIRE(std::abs(params.masterGainDb - (-24.0f)) < 1e-4f);
    REQUIRE_FALSE(applyCcToParams(params, 1, 64));    // dynamics is native
    REQUIRE_FALSE(applyCcToParams(params, 74, 64));   // sappsynth's CC, not ours
}

TEST_CASE("CC 16 in a rendered clip actually moves the source across the stage", "[sapplink]")
{
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.4f, 5});

    auto renderWithStageCc = [&](int ccValue) {
        std::vector<sapp::sounds::TimedMidiEvent> song;
        song.push_back({0.0, 0xB0, 0, 16, uint8_t(ccValue), 0});  // SappLink stageX
        song.push_back({0.3, 0x90, 0, 60, 100, 0});
        song.push_back({1.6, 0x80, 0, 60, 0, 0});
        OrchestraRenderOptions options;
        options.tailSeconds = 0.4;
        options.params.tailLevel = 0.0f;   // isolate the direct path
        options.params.earlyLevel = 0.0f;
        return renderOrchestra(inst, song, options);
    };

    auto energies = [](const OrchestraRenderOutput& out) {
        double l = 0, r = 0;
        for (size_t i = 0; i < out.left.size(); ++i) {
            l += double(out.left[i]) * out.left[i];
            r += double(out.right[i]) * out.right[i];
        }
        return std::pair<double, double>(l, r);
    };

    const auto [hardLeftL, hardLeftR] = energies(renderWithStageCc(0));
    const auto [hardRightL, hardRightR] = energies(renderWithStageCc(127));
    REQUIRE(hardLeftL > hardLeftR * 1.5);    // CC 16 = 0  → left-heavy
    REQUIRE(hardRightR > hardRightL * 1.5);  // CC 16 = 127 → right-heavy
}

TEST_CASE("CC 7 in a rendered clip scales output level", "[sapplink]")
{
    auto inst = sapp::sounds::makeDiagnosticInstrument({48000, 1.0f, 0.4f, 5});

    auto renderWithMasterCc = [&](int ccValue) {
        std::vector<sapp::sounds::TimedMidiEvent> song;
        song.push_back({0.0, 0xB0, 0, 7, uint8_t(ccValue), 0});
        song.push_back({0.2, 0x90, 0, 60, 100, 0});
        song.push_back({1.2, 0x80, 0, 60, 0, 0});
        OrchestraRenderOptions options;
        options.tailSeconds = 0.3;
        options.params.limiter = false;
        return renderOrchestra(inst, song, options);
    };

    const float quiet = renderWithMasterCc(0).rms;    // -24 dB
    const float loud = renderWithMasterCc(127).rms;   // +12 dB
    REQUIRE(loud > quiet * 10.0f);
}
