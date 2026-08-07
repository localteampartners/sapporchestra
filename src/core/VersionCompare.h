#pragma once
// Tiny semver-ish comparison for the in-plugin updater. Framework-free so
// the unit tests cover it. Accepts "1.2.3" or "v1.2.3".

#include <cstdlib>

namespace sapp::orchestra {

// true when `candidate` is strictly newer than `current`.
inline bool isNewerVersion(const char* candidate, const char* current)
{
    auto parse = [](const char* s, int out[3]) {
        if (*s == 'v' || *s == 'V') ++s;
        for (int i = 0; i < 3; ++i) {
            char* end = nullptr;
            out[i] = int(std::strtol(s, &end, 10));
            if (end == s) out[i] = 0;
            s = (end != nullptr && *end == '.') ? end + 1 : (end != nullptr ? end : s);
        }
    };
    int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
    parse(candidate, a);
    parse(current, b);
    for (int i = 0; i < 3; ++i) {
        if (a[i] > b[i]) return true;
        if (a[i] < b[i]) return false;
    }
    return false;
}

} // namespace sapp::orchestra
