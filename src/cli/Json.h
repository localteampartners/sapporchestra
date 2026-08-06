#pragma once
// Tiny JSON writer for the SappOrchestra CLI (mirrors SappSounds tools/common).

#include <sstream>
#include <string>
#include <vector>

namespace sapptools {

inline std::string jsonEscape(const std::string& s)
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
                if (uint8_t(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

// Streaming writer producing compact, valid JSON.
class JsonWriter {
public:
    void beginObject() { comma(); out_ << '{'; stack_.push_back(true); first_ = true; }
    void endObject() { out_ << '}'; stack_.pop_back(); first_ = false; }
    void beginArray() { comma(); out_ << '['; stack_.push_back(true); first_ = true; }
    void endArray() { out_ << ']'; stack_.pop_back(); first_ = false; }

    void key(const std::string& k)
    {
        comma();
        out_ << '"' << jsonEscape(k) << "\":";
        pendingKey_ = true;
    }
    void value(const std::string& v) { comma(); out_ << '"' << jsonEscape(v) << '"'; }
    void value(const char* v) { value(std::string(v)); }
    void value(bool v) { comma(); out_ << (v ? "true" : "false"); }
    void value(double v) { comma(); out_ << v; }
    void value(int64_t v) { comma(); out_ << v; }
    void value(uint64_t v) { comma(); out_ << v; }
    void value(int v) { comma(); out_ << v; }
    void value(unsigned v) { comma(); out_ << v; }

    template <typename T>
    void field(const std::string& k, T v) { key(k); value(v); }

    std::string str() const { return out_.str(); }

private:
    void comma()
    {
        if (pendingKey_) { pendingKey_ = false; return; }
        if (!first_ && !stack_.empty()) out_ << ',';
        first_ = false;
    }
    std::ostringstream out_;
    std::vector<bool> stack_;
    bool first_ = true;
    bool pendingKey_ = false;
};

} // namespace sapptools
