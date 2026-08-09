#pragma once
// SFZ library enumeration for the host-automatable `instrument` parameter
// (sapptune issue #20). Scans a samples root for playable .sfz instruments,
// caches the result in a JSON index at <root>/.sapp-sfz-index.json (same
// pattern as sappstep's .sappstep-index.json), and defines the ONE ordering
// contract shared by the plugin's choice list, the CLI, and any driving
// session: entries sorted case-insensitively by label. A mismatched sort
// order silently automates the wrong sound, so the order lives here and
// nowhere else.
//
// Framework-independent (no JUCE): consumed by the plugin, the CLI, and the
// unit tests. File format and mapping contract are documented in the
// SappLink manifest (sapptune/sapplink/manifests/sapporchestra.json,
// "instrumentSelect").

#include <string>
#include <vector>

namespace sapp::sfzlib {

struct Entry {
    std::string path;   // absolute path to the .sfz file
    std::string label;  // path relative to the root, '/'-separated, no ".sfz"
};

/// Index file cached at the samples root. Shared by every Sapp SFZ sampler
/// (sapporchestra, sappchoir): both scan the same root with the same rules,
/// so both see the same list in the same order.
inline constexpr const char* kIndexFileName = ".sapp-sfz-index.json";
inline constexpr int kIndexVersion = 1;

/// Environment override for the samples root (wins over the shared Sapp
/// "samplesRoot" setting). Lets headless tests and driving sessions pin the
/// library without touching user settings.
inline constexpr const char* kRootEnvVar = "SAPP_SFZ_ROOT";

/// $SAPP_SFZ_ROOT if set and non-empty, else `fallback`.
std::string resolveRoot(const std::string& fallback);

/// THE ordering contract: case-insensitive (ASCII) compare of `label`,
/// ties broken by exact label bytes, then by path. Deterministic on any
/// machine for any scan order.
bool entryLess(const Entry& a, const Entry& b);

/// Recursive scan of `root` for playable .sfz instruments (files that parse
/// to at least one region). Skips dot-hidden files/directories and anything
/// under a directory named "includes" or "modules" (SFZ fragment
/// conventions). Result is sorted with entryLess.
std::vector<Entry> scan(const std::string& root);

/// Write <root>/.sapp-sfz-index.json. Best-effort; false if unwritable.
bool writeIndex(const std::string& root, const std::vector<Entry>& entries);

/// Read the index back. False when missing/unreadable/wrong version. The
/// result is re-sorted with entryLess, so a hand-edited file cannot change
/// the choice order.
bool loadIndex(const std::string& root, std::vector<Entry>& out);

/// Fast startup path: loadIndex, else scan + writeIndex. This is what the
/// plugin calls at construction; the choice list built from it is FIXED for
/// the lifetime of that instance (JUCE cannot grow a choice list live), so
/// a rescan takes effect on the next instantiation.
std::vector<Entry> loadOrScan(const std::string& root);

} // namespace sapp::sfzlib
