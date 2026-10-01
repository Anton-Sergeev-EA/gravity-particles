#include "I18n.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "Json.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace fs = std::filesystem;

namespace {

bool readTextFile(const fs::path& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

void flatten(const JsonValue& node, const std::string& prefix,
             std::unordered_map<std::string, std::string>& out) {
    if (node.isObject()) {
        for (const auto& kv : node.object) {
            flatten(kv.second, prefix.empty() ? kv.first : prefix + "." + kv.first, out);
        }
    } else if (node.isString()) {
        out[prefix] = node.string;
    }
}

std::string stringOr(const JsonValue& obj, const char* key, const std::string& def) {
    const JsonValue* v = obj.find(key);
    return (v && v->isString()) ? v->string : def;
}

}  // namespace

bool I18n::load(const fs::path& localesDir, std::string* error) {
    auto setError = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };

    std::string text;
    const fs::path manifestPath = localesDir / "languages.json";
    if (!readTextFile(manifestPath, text)) return setError("cannot read " + manifestPath.string());

    JsonValue manifest;
    try {
        manifest = parseJson(text);
    } catch (const JsonError& e) {
        return setError(manifestPath.string() + ": " + e.what());
    }
    const JsonValue* order = manifest.find("order");
    if (!order || !order->isArray() || order->array.empty()) {
        return setError(manifestPath.string() + ": missing \"order\" array");
    }

    languages_.clear();
    tables_.clear();
    for (const JsonValue& codeValue : order->array) {
        if (!codeValue.isString()) continue;
        const std::string& code = codeValue.string;
        const fs::path path = localesDir / (code + ".json");
        if (!readTextFile(path, text)) return setError("cannot read " + path.string());
        JsonValue doc;
        try {
            doc = parseJson(text);
        } catch (const JsonError& e) {
            return setError(path.string() + ": " + e.what());
        }
        Table table;
        flatten(doc, "", table);
        LanguageInfo info;
        info.code = code;
        info.name = table.count("meta.name") ? table["meta.name"] : code;
        info.englishName = table.count("meta.english_name") ? table["meta.english_name"] : code;
        languages_.push_back(info);
        tables_[code] = std::move(table);
    }

    default_ = stringOr(manifest, "default", languages_.front().code);
    fallback_ = stringOr(manifest, "fallback", default_);
    if (!isSupported(default_)) default_ = languages_.front().code;
    if (!isSupported(fallback_)) fallback_ = default_;
    current_ = default_;
    return true;
}

bool I18n::isSupported(std::string_view code) const {
    return tables_.find(std::string(code)) != tables_.end();
}

bool I18n::setLanguage(std::string_view code) {
    if (!isSupported(code)) return false;
    current_ = std::string(code);
    return true;
}

int I18n::languageIndex() const {
    for (size_t i = 0; i < languages_.size(); ++i) {
        if (languages_[i].code == current_) return static_cast<int>(i);
    }
    return 0;
}

const LanguageInfo* I18n::languageInfo(std::string_view code) const {
    for (const auto& l : languages_) {
        if (l.code == code) return &l;
    }
    return nullptr;
}

const std::string* I18n::raw(std::string_view language, std::string_view key) const {
    auto t = tables_.find(std::string(language));
    if (t == tables_.end()) return nullptr;
    auto it = t->second.find(std::string(key));
    return it == t->second.end() ? nullptr : &it->second;
}

bool I18n::has(std::string_view language, std::string_view key) const {
    return raw(language, key) != nullptr;
}

std::vector<std::string> I18n::keys(std::string_view language) const {
    std::vector<std::string> out;
    auto t = tables_.find(std::string(language));
    if (t == tables_.end()) return out;
    out.reserve(t->second.size());
    for (const auto& kv : t->second) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

std::string I18n::tr(std::string_view key) const {
    if (const std::string* s = raw(current_, key)) return *s;
    if (const std::string* s = raw(fallback_, key)) return *s;
    return std::string(key);
}

std::string I18n::tr(std::string_view key, const std::vector<std::string>& args) const {
    return format(tr(key), args);
}

std::string I18n::format(std::string_view pattern, const std::vector<std::string>& args) {
    std::string out;
    out.reserve(pattern.size() + 16);
    for (size_t i = 0; i < pattern.size(); ++i) {
        char c = pattern[i];
        if (c == '{') {
            size_t close = pattern.find('}', i + 1);
            if (close != std::string_view::npos && close > i + 1) {
                std::string_view idx = pattern.substr(i + 1, close - i - 1);
                bool digits = std::all_of(idx.begin(), idx.end(),
                                          [](char d) { return d >= '0' && d <= '9'; });
                if (digits && idx.size() <= 2) {
                    size_t n = static_cast<size_t>(std::atoi(std::string(idx).c_str()));
                    if (n < args.size()) out += args[n];
                    i = close;
                    continue;
                }
            }
        }
        out.push_back(c);
    }
    return out;
}

std::string I18n::formatInt(long long value) const {
    const std::string sep = tr("meta.group_separator");
    const std::string grouping = tr("meta.grouping");
    // "3" — западная группировка, "3;2" — индийская (последние 3 цифры, затем по 2).
    int first = 3, rest = 3;
    if (grouping == "3;2") rest = 2;

    bool negative = value < 0;
    unsigned long long v = negative ? 0ULL - static_cast<unsigned long long>(value)
                                    : static_cast<unsigned long long>(value);
    std::string digits = std::to_string(v);

    // По типографским правилам ряда языков (ru, es) четырёхзначные числа не
    // разбивают на группы: «6000», но «12 000». Порог задаётся в локали.
    size_t minDigits = 4;
    const std::string minText = tr("meta.group_min_digits");
    if (minText != "meta.group_min_digits") {
        minDigits = static_cast<size_t>(std::clamp(std::atoi(minText.c_str()), 2, 9));
    }
    std::string out;
    if (digits.size() < minDigits || sep == "meta.group_separator") {
        out = digits;
    } else {
        std::vector<std::string> groups;
        size_t end = digits.size();
        size_t size = static_cast<size_t>(first);
        while (end > 0) {
            size_t start = end > size ? end - size : 0;
            groups.push_back(digits.substr(start, end - start));
            end = start;
            size = static_cast<size_t>(rest);
        }
        for (size_t i = groups.size(); i-- > 0;) {
            out += groups[i];
            if (i != 0) out += sep;
        }
    }
    return negative ? "-" + out : out;
}

std::string I18n::formatFloat(double value, int decimals) const {
    if (!std::isfinite(value)) return "—";
    decimals = std::clamp(decimals, 0, 6);
    double scale = std::pow(10.0, decimals);
    double rounded = std::round(std::fabs(value) * scale) / scale;
    long long whole = static_cast<long long>(std::floor(rounded));
    std::string out = formatInt(whole);
    if (decimals > 0) {
        long long frac = static_cast<long long>(std::llround((rounded - static_cast<double>(whole)) * scale));
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%0*lld", decimals, frac);
        std::string dec = tr("meta.decimal_separator");
        if (dec == "meta.decimal_separator") dec = ".";
        out += dec + buf;
    }
    return (value < 0 && rounded != 0.0) ? "-" + out : out;
}

std::string I18n::normalizeTag(std::string_view tag) {
    std::string t;
    for (char c : tag) {
        if (c == '.' || c == '@') break;  // ru_RU.UTF-8, sr_RS@latin
        t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (t.empty() || t == "c" || t == "posix") return "";
    size_t cut = t.find_first_of("_-");
    std::string primary = cut == std::string::npos ? t : t.substr(0, cut);
    // Трёхбуквенные коды ISO 639-2, которые встречаются в системах.
    if (primary == "rus") return "ru";
    if (primary == "eng") return "en";
    if (primary == "zho" || primary == "chi" || primary == "cmn") return "zh";
    if (primary == "hin") return "hi";
    if (primary == "spa") return "es";
    if (primary == "fra" || primary == "fre") return "fr";
    if (primary == "deu" || primary == "ger") return "de";
    if (primary == "ita") return "it";
    return primary;
}

std::vector<std::string> I18n::systemLanguageTags() {
    std::vector<std::string> tags;
    auto addList = [&](const char* value, char delim) {
        if (!value) return;
        std::string s(value);
        size_t start = 0;
        while (start <= s.size()) {
            size_t end = s.find(delim, start);
            if (end == std::string::npos) end = s.size();
            if (end > start) tags.push_back(s.substr(start, end - start));
            start = end + 1;
        }
    };

    // Переменные окружения (POSIX). LANGUAGE — список через двоеточие.
    addList(std::getenv("LANGUAGE"), ':');
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const char* v = std::getenv(var)) tags.emplace_back(v);
    }

#if defined(_WIN32)
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0) {
        std::string ascii;
        for (const wchar_t* p = name; *p; ++p) ascii.push_back(static_cast<char>(*p & 0x7F));
        tags.push_back(ascii);
    }
#elif defined(__APPLE__)
    if (CFArrayRef langs = CFLocaleCopyPreferredLanguages()) {
        CFIndex n = CFArrayGetCount(langs);
        for (CFIndex i = 0; i < n; ++i) {
            auto str = static_cast<CFStringRef>(CFArrayGetValueAtIndex(langs, i));
            char buf[64] = {};
            if (str && CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                tags.emplace_back(buf);
            }
        }
        CFRelease(langs);
    }
#endif
    return tags;
}

std::string I18n::detectSystemLanguage() const {
    for (const std::string& tag : systemLanguageTags()) {
        std::string code = normalizeTag(tag);
        if (!code.empty() && isSupported(code)) return code;
    }
    return default_;
}
