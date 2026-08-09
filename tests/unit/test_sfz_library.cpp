#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/SfzLibrary.h"

// SfzLibrary backs the host-automatable `instrument` choice parameter
// (sapptune issue #20). The ordering contract is the load-bearing part: the
// plugin's choice list, the on-disk index, and a driving session's
// name->index mapping must all agree, or automation selects the wrong sound.

using namespace sapp::sfzlib;
namespace fs = std::filesystem;

static const std::string kFixture = std::string(SAPPORCH_TEST_DATA_DIR) + "/sfz-library";

TEST_CASE("scan finds playable instruments, sorted case-insensitively", "[sfzlib]")
{
    const auto entries = scan(kFixture);
    REQUIRE(entries.size() == 3);

    // Case-insensitive by label: "acme/Beta Flute" < "Gamma" <
    // "Zeta Lib/alpha trumpet". A case-SENSITIVE byte sort would put
    // "Gamma" (0x47) first — this ordering is the regression guard.
    REQUIRE(entries[0].label == "acme/Beta Flute");
    REQUIRE(entries[1].label == "Gamma");
    REQUIRE(entries[2].label == "Zeta Lib/alpha trumpet");

    for (const auto& entry : entries) {
        REQUIRE(fs::exists(entry.path));
        REQUIRE(entry.path.find(".sfz") != std::string::npos);
    }

    // The scan's own order must satisfy the contract comparator.
    REQUIRE(std::is_sorted(entries.begin(), entries.end(), entryLess));
}

TEST_CASE("scan skips fragments, hidden files, and region-less sfz", "[sfzlib]")
{
    const auto entries = scan(kFixture);
    for (const auto& entry : entries) {
        REQUIRE(entry.label.find("includes/") == std::string::npos);   // fragment dir
        REQUIRE(entry.label.find(".hidden") == std::string::npos);     // hidden dir
        REQUIRE(entry.label.find("empty") == std::string::npos);       // zero regions
    }
}

TEST_CASE("index round-trips and re-sorts on load", "[sfzlib]")
{
    const auto entries = scan(kFixture);
    const auto tmp = fs::temp_directory_path() / "sapporch-sfzlib-test";
    fs::create_directories(tmp);

    REQUIRE(writeIndex(tmp.string(), entries));
    std::vector<Entry> loaded;
    REQUIRE(loadIndex(tmp.string(), loaded));
    REQUIRE(loaded.size() == entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        REQUIRE(loaded[i].label == entries[i].label);
        REQUIRE(loaded[i].path == entries[i].path);
    }

    // A hand-shuffled index must come back in contract order: write the
    // entries reversed and check loadIndex undoes it.
    auto reversed = entries;
    std::reverse(reversed.begin(), reversed.end());
    REQUIRE(writeIndex(tmp.string(), reversed));
    loaded.clear();
    REQUIRE(loadIndex(tmp.string(), loaded));
    REQUIRE(std::is_sorted(loaded.begin(), loaded.end(), entryLess));
    REQUIRE(loaded[0].label == entries[0].label);

    fs::remove_all(tmp);
}

TEST_CASE("index survives paths with quotes and backslashes", "[sfzlib]")
{
    const auto tmp = fs::temp_directory_path() / "sapporch-sfzlib-esc";
    fs::create_directories(tmp);
    std::vector<Entry> weird{
        {R"(C:\Samples\odd "name".sfz)", R"(odd "name")"},
        {"/a/b\tc.sfz", "b\tc"},
    };
    std::sort(weird.begin(), weird.end(), entryLess);
    REQUIRE(writeIndex(tmp.string(), weird));
    std::vector<Entry> loaded;
    REQUIRE(loadIndex(tmp.string(), loaded));
    REQUIRE(loaded.size() == 2);
    REQUIRE(loaded[0].path == weird[0].path);
    REQUIRE(loaded[1].path == weird[1].path);
    fs::remove_all(tmp);
}

TEST_CASE("loadIndex rejects a wrong version", "[sfzlib]")
{
    const auto tmp = fs::temp_directory_path() / "sapporch-sfzlib-ver";
    fs::create_directories(tmp);
    std::ofstream(tmp / kIndexFileName)
        << R"({"sappSfzIndex": 999, "entries": [{"label": "x", "path": "/x.sfz"}]})";
    std::vector<Entry> loaded;
    REQUIRE_FALSE(loadIndex(tmp.string(), loaded));
    fs::remove_all(tmp);
}

TEST_CASE("resolveRoot prefers the environment override", "[sfzlib]")
{
    ::setenv(kRootEnvVar, "/tmp/somewhere-else", 1);
    REQUIRE(resolveRoot("/fallback") == "/tmp/somewhere-else");
    ::unsetenv(kRootEnvVar);
    REQUIRE(resolveRoot("/fallback") == "/fallback");
}

TEST_CASE("loadOrScan caches: second call reads the index it wrote", "[sfzlib]")
{
    // Copy the fixture so the cache write cannot pollute tests/data.
    const auto tmp = fs::temp_directory_path() / "sapporch-sfzlib-cache";
    fs::remove_all(tmp);
    fs::copy(kFixture, tmp, fs::copy_options::recursive);

    const auto first = loadOrScan(tmp.string());
    REQUIRE(first.size() == 3);
    REQUIRE(fs::exists(tmp / kIndexFileName));

    // Delete the .sfz files: a cached index must still enumerate (startup
    // speed is the point), and the plugin handles missing files gracefully.
    fs::remove(tmp / "Gamma.sfz");
    const auto second = loadOrScan(tmp.string());
    REQUIRE(second.size() == 3);
    fs::remove_all(tmp);
}
