#include "SfzLibrary.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <sapp/sounds/SfzParser.h>

namespace sapp::sfzlib {

namespace fs = std::filesystem;

// ------------------------------------------------------------------ order --

static char asciiLower(char c)
{
    return char(std::tolower(static_cast<unsigned char>(c)));
}

bool entryLess(const Entry& a, const Entry& b)
{
    const size_t n = std::min(a.label.size(), b.label.size());
    for (size_t i = 0; i < n; ++i) {
        const char la = asciiLower(a.label[i]);
        const char lb = asciiLower(b.label[i]);
        if (la != lb) return la < lb;
    }
    if (a.label.size() != b.label.size()) return a.label.size() < b.label.size();
    if (a.label != b.label) return a.label < b.label;
    return a.path < b.path;
}

// ------------------------------------------------------------------- root --

std::string resolveRoot(const std::string& fallback)
{
    if (const char* env = std::getenv(kRootEnvVar))
        if (env[0] != 0) return env;
    return fallback;
}

// ------------------------------------------------------------------- scan --

static bool isHidden(const fs::path& p)
{
    const auto name = p.filename().string();
    return !name.empty() && name[0] == '.';
}

static bool underFragmentDir(const fs::path& fileRelative)
{
    // "includes"/"modules" directories conventionally hold #include fragments,
    // not playable instruments (same rule as the `scan` CLI command and the
    // factory-preset loader).
    for (const auto& part : fileRelative.parent_path()) {
        std::string s = part.string();
        std::transform(s.begin(), s.end(), s.begin(), asciiLower);
        if (s == "includes" || s == "modules") return true;
        if (!s.empty() && s[0] == '.') return true;  // hidden directory
    }
    return false;
}

std::vector<Entry> scan(const std::string& root)
{
    std::vector<Entry> entries;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return entries;

    sapp::sounds::SfzParser parser;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file(ec)) continue;
        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), asciiLower);
        if (ext != ".sfz") continue;
        if (isHidden(it->path())) continue;

        const auto relative = fs::relative(it->path(), root, ec);
        if (ec || underFragmentDir(relative)) continue;

        // Playability filter: a file that parses to zero regions is a
        // fragment or broken and would load as silence — leave it out so
        // every choice index maps to a real sound.
        auto parsed = parser.parseFile(it->path());
        if (parsed.instrument.regions.empty()) continue;

        Entry entry;
        entry.path = it->path().string();
        auto label = relative.generic_string();
        if (label.size() > 4) label.erase(label.size() - 4);  // strip ".sfz"
        entry.label = label;
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), entryLess);
    return entries;
}

// ------------------------------------------------------------------ index --

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

/// Reads the JSON string whose opening quote is at text[pos]. Returns the
/// unescaped value and leaves `pos` after the closing quote. Handles the
/// escapes jsonEscape produces (plus \/ and \uXXXX for ASCII).
static bool readJsonString(const std::string& text, size_t& pos, std::string& out)
{
    if (pos >= text.size() || text[pos] != '"') return false;
    ++pos;
    out.clear();
    while (pos < text.size()) {
        const char c = text[pos];
        if (c == '"') { ++pos; return true; }
        if (c == '\\') {
            if (pos + 1 >= text.size()) return false;
            const char esc = text[pos + 1];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (pos + 5 >= text.size()) return false;
                    const unsigned code = unsigned(
                        std::strtoul(text.substr(pos + 2, 4).c_str(), nullptr, 16));
                    if (code < 0x80) out += char(code);
                    // (non-ASCII \u escapes never appear in files we write)
                    pos += 4;
                    break;
                }
                default: return false;
            }
            pos += 2;
        } else {
            out += c;
            ++pos;
        }
    }
    return false;
}

/// Finds `"key"` at/after pos and positions the cursor on the opening quote
/// of its string value. Returns false when the key is absent.
static bool seekStringValue(const std::string& text, size_t& pos, const char* key)
{
    const std::string needle = std::string("\"") + key + "\"";
    const size_t k = text.find(needle, pos);
    if (k == std::string::npos) return false;
    size_t p = text.find(':', k + needle.size());
    if (p == std::string::npos) return false;
    ++p;
    while (p < text.size() && (text[p] == ' ' || text[p] == '\n' || text[p] == '\r' || text[p] == '\t'))
        ++p;
    if (p >= text.size() || text[p] != '"') return false;
    pos = p;
    return true;
}

bool writeIndex(const std::string& root, const std::vector<Entry>& entries)
{
    const auto file = fs::path(root) / kIndexFileName;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out.good()) return false;
    out << "{\n";
    out << "  \"sappSfzIndex\": " << kIndexVersion << ",\n";
    out << "  \"root\": \"" << jsonEscape(root) << "\",\n";
    out << "  \"order\": \"case-insensitive by label; see sapplink manifest instrumentSelect\",\n";
    out << "  \"count\": " << entries.size() << ",\n";
    out << "  \"entries\": [";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << (i == 0 ? "\n" : ",\n");
        out << "    {\"label\": \"" << jsonEscape(entries[i].label)
            << "\", \"path\": \"" << jsonEscape(entries[i].path) << "\"}";
    }
    out << "\n  ]\n}\n";
    return out.good();
}

bool loadIndex(const std::string& root, std::vector<Entry>& out)
{
    out.clear();
    const auto file = fs::path(root) / kIndexFileName;
    std::ifstream in(file, std::ios::binary);
    if (!in.good()) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();

    // Version gate: refuse formats we do not understand.
    const size_t v = text.find("\"sappSfzIndex\"");
    if (v == std::string::npos) return false;
    const size_t colon = text.find(':', v);
    if (colon == std::string::npos || std::atoi(text.c_str() + colon + 1) != kIndexVersion)
        return false;

    size_t pos = text.find("\"entries\"");
    if (pos == std::string::npos) return false;
    while (true) {
        size_t labelPos = pos;
        if (!seekStringValue(text, labelPos, "label")) break;
        Entry entry;
        if (!readJsonString(text, labelPos, entry.label)) return false;
        size_t pathPos = labelPos;
        if (!seekStringValue(text, pathPos, "path")) return false;
        if (!readJsonString(text, pathPos, entry.path)) return false;
        out.push_back(std::move(entry));
        pos = pathPos;
    }
    // Re-sort: the ordering contract lives in entryLess, not in whatever
    // order the file happens to have (hand edits must not shift indices).
    std::sort(out.begin(), out.end(), entryLess);
    return true;
}

std::vector<Entry> loadOrScan(const std::string& root)
{
    std::vector<Entry> entries;
    if (loadIndex(root, entries)) return entries;
    entries = scan(root);
    if (!entries.empty()) writeIndex(root, entries);  // best-effort cache
    return entries;
}

} // namespace sapp::sfzlib
