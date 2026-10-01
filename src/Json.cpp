#include "Json.h"

#include <cstdlib>

#include "Utf8.h"

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : object) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

JsonError::JsonError(const std::string& message, int line_, int column_)
    : std::runtime_error(message + " (line " + std::to_string(line_) + ", column " +
                         std::to_string(column_) + ")"),
      line(line_),
      column(column_) {}

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    JsonValue parseDocument() {
        // Пропускаем UTF-8 BOM, который иногда оставляют редакторы Windows.
        if (s_.size() >= 3 && static_cast<unsigned char>(s_[0]) == 0xEF &&
            static_cast<unsigned char>(s_[1]) == 0xBB && static_cast<unsigned char>(s_[2]) == 0xBF) {
            pos_ = 3;
        }
        JsonValue v = parseValue(0);
        skipWs();
        if (pos_ != s_.size()) fail("unexpected trailing characters");
        return v;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;
    static constexpr int kMaxDepth = 64;

    [[noreturn]] void fail(const std::string& msg) const {
        int line = 1, col = 1;
        for (size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') {
                ++line;
                col = 1;
            } else if ((static_cast<unsigned char>(s_[i]) & 0xC0) != 0x80) {
                ++col;
            }
        }
        throw JsonError(msg, line, col);
    }

    void skipWs() {
        while (pos_ < s_.size() &&
               (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool consume(char c) {
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char c) {
        if (!consume(c)) fail(std::string("expected '") + c + "'");
    }

    bool matchWord(const char* word) {
        size_t n = 0;
        while (word[n]) ++n;
        if (s_.compare(pos_, n, word) == 0) {
            pos_ += n;
            return true;
        }
        return false;
    }

    JsonValue parseValue(int depth) {
        if (depth > kMaxDepth) fail("nesting too deep");
        skipWs();
        if (pos_ >= s_.size()) fail("unexpected end of input");
        char c = s_[pos_];
        JsonValue v;
        if (c == '{') {
            ++pos_;
            v.type = JsonValue::Type::Object;
            if (consume('}')) return v;
            do {
                skipWs();
                if (pos_ >= s_.size() || s_[pos_] != '"') fail("expected string key");
                std::string key = parseString();
                expect(':');
                v.object.emplace_back(std::move(key), parseValue(depth + 1));
            } while (consume(','));
            expect('}');
        } else if (c == '[') {
            ++pos_;
            v.type = JsonValue::Type::Array;
            if (consume(']')) return v;
            do {
                v.array.push_back(parseValue(depth + 1));
            } while (consume(','));
            expect(']');
        } else if (c == '"') {
            v.type = JsonValue::Type::String;
            v.string = parseString();
        } else if (matchWord("true")) {
            v.type = JsonValue::Type::Bool;
            v.boolean = true;
        } else if (matchWord("false")) {
            v.type = JsonValue::Type::Bool;
        } else if (matchWord("null")) {
            v.type = JsonValue::Type::Null;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            const char* begin = s_.c_str() + pos_;
            char* end = nullptr;
            v.type = JsonValue::Type::Number;
            v.number = std::strtod(begin, &end);
            if (end == begin) fail("invalid number");
            pos_ += static_cast<size_t>(end - begin);
        } else {
            fail("unexpected character");
        }
        return v;
    }

    unsigned parseHex4() {
        if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            char h = s_[pos_++];
            value <<= 4;
            if (h >= '0' && h <= '9') value |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') value |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') value |= static_cast<unsigned>(h - 'A' + 10);
            else fail("invalid hex digit in \\u escape");
        }
        return value;
    }

    std::string parseString() {
        ++pos_;  // открывающая кавычка
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') break;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= s_.size()) fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    char32_t cp = parseHex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 2 > s_.size() || s_[pos_] != '\\' || s_[pos_ + 1] != 'u') {
                            fail("unpaired surrogate");
                        }
                        pos_ += 2;
                        char32_t lo = parseHex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("unpaired surrogate");
                    }
                    utf8::append(out, cp);
                    break;
                }
                default: fail("invalid escape");
            }
        }
        return out;
    }
};

}  // namespace

JsonValue parseJson(const std::string& text) {
    return Parser(text).parseDocument();
}
