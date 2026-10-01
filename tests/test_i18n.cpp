#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "I18n.h"
#include "Test.h"

namespace fs = std::filesystem;

namespace {

const fs::path kLocales = fs::path(GP_SOURCE_DIR) / "locales";

I18n loadCatalog() {
    I18n i18n;
    std::string error;
    if (!i18n.load(kLocales, &error)) test::fail(__FILE__, __LINE__, "load failed: " + error);
    return i18n;
}

std::set<std::string> placeholders(const std::string& s) {
    std::set<std::string> out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '{') continue;
        size_t close = s.find('}', i);
        if (close != std::string::npos) out.insert(s.substr(i, close - i + 1));
    }
    return out;
}

}  // namespace

TEST(i18n_language_order_russian_first) {
    I18n i18n = loadCatalog();
    const char* expected[] = {"ru", "en", "zh", "hi", "es", "fr", "de", "it"};
    CHECK_EQ(i18n.languages().size(), size_t(8));
    for (size_t i = 0; i < i18n.languages().size() && i < 8; ++i) {
        CHECK_EQ(i18n.languages()[i].code, std::string(expected[i]));
    }
    CHECK_EQ(i18n.defaultLanguage(), std::string("ru"));
    CHECK_EQ(i18n.fallbackLanguage(), std::string("ru"));
    CHECK_EQ(i18n.language(), std::string("ru"));
}

TEST(i18n_native_language_names) {
    I18n i18n = loadCatalog();
    CHECK_EQ(i18n.languageInfo("ru")->name, std::string("Русский"));
    CHECK_EQ(i18n.languageInfo("zh")->name, std::string("中文"));
    CHECK_EQ(i18n.languageInfo("hi")->name, std::string("हिन्दी"));
    CHECK_EQ(i18n.languageInfo("de")->name, std::string("Deutsch"));
}

// Каждый перевод содержит ровно те же ключи, что и русский (основной) —
// ни пропусков, ни опечаток в именах ключей.
TEST(i18n_all_locales_complete) {
    I18n i18n = loadCatalog();
    const auto reference = i18n.keys("ru");
    CHECK(reference.size() > 80);
    for (const auto& lang : i18n.languages()) {
        const auto keys = i18n.keys(lang.code);
        for (const auto& k : reference) {
            if (!i18n.has(lang.code, k)) test::fail(__FILE__, __LINE__, lang.code + ": missing key " + k);
        }
        for (const auto& k : keys) {
            if (!i18n.has("ru", k)) test::fail(__FILE__, __LINE__, lang.code + ": unknown key " + k);
        }
    }
}

TEST(i18n_placeholders_match_reference) {
    I18n i18n = loadCatalog();
    for (const auto& lang : i18n.languages()) {
        for (const auto& k : i18n.keys("ru")) {
            const std::string* ref = i18n.raw("ru", k);
            const std::string* tr = i18n.raw(lang.code, k);
            if (!ref || !tr) continue;
            if (placeholders(*ref) != placeholders(*tr)) {
                test::fail(__FILE__, __LINE__, lang.code + ": placeholder mismatch in " + k);
            }
            if (tr->empty()) test::fail(__FILE__, __LINE__, lang.code + ": empty string " + k);
        }
    }
}

TEST(i18n_translate_and_switch) {
    I18n i18n = loadCatalog();
    CHECK_EQ(i18n.tr("panel.title"), std::string("Настройки"));
    CHECK(i18n.setLanguage("zh"));
    CHECK_EQ(i18n.tr("panel.title"), std::string("设置"));
    CHECK(i18n.setLanguage("hi"));
    CHECK_EQ(i18n.tr("panel.language"), std::string("भाषा"));
    CHECK(!i18n.setLanguage("xx"));
    CHECK_EQ(i18n.language(), std::string("hi"));
    CHECK_EQ(i18n.tr("no.such.key"), std::string("no.such.key"));
    CHECK(i18n.setLanguage("en"));
    CHECK_EQ(i18n.tr("toast.language", {"Deutsch"}), std::string("Language: Deutsch"));
}

TEST(i18n_falls_back_to_russian) {
    const fs::path dir = fs::temp_directory_path() / "gp_i18n_fallback_test";
    fs::create_directories(dir);
    std::ofstream(dir / "languages.json") << R"({"default": "ru", "fallback": "ru", "order": ["ru", "xx"]})";
    std::ofstream(dir / "ru.json") << R"({"meta": {"name": "Русский"}, "a": "А", "b": "Б"})";
    std::ofstream(dir / "xx.json") << R"({"meta": {"name": "Test"}, "a": "A"})";
    I18n i18n;
    CHECK(i18n.load(dir));
    CHECK(i18n.setLanguage("xx"));
    CHECK_EQ(i18n.tr("a"), std::string("A"));
    CHECK_EQ(i18n.tr("b"), std::string("Б"));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST(i18n_format_placeholders) {
    CHECK_EQ(I18n::format("{0} + {1} = {0}", {"a", "b"}), std::string("a + b = a"));
    CHECK_EQ(I18n::format("{2} missing", {"a"}), std::string(" missing"));
    CHECK_EQ(I18n::format("{x} {} {", {"a"}), std::string("{x} {} {"));
}

TEST(i18n_number_formatting_per_locale) {
    I18n i18n = loadCatalog();
    const std::string nbsp = "\xC2\xA0", nnbsp = "\xE2\x80\xAF";
    i18n.setLanguage("ru");
    CHECK_EQ(i18n.formatInt(6000), std::string("6000"));  // 4 знака не разбиваются
    CHECK_EQ(i18n.formatInt(12000), "12" + nbsp + "000");
    CHECK_EQ(i18n.formatFloat(1.4, 1), std::string("1,4"));
    CHECK_EQ(i18n.formatInt(-1234567), "-1" + nbsp + "234" + nbsp + "567");
    i18n.setLanguage("en");
    CHECK_EQ(i18n.formatInt(6000), std::string("6,000"));
    CHECK_EQ(i18n.formatFloat(0.25, 2), std::string("0.25"));
    CHECK_EQ(i18n.formatFloat(-0.04, 1), std::string("0.0"));
    i18n.setLanguage("hi");
    CHECK_EQ(i18n.formatInt(1234567), std::string("12,34,567"));  // индийская группировка
    CHECK_EQ(i18n.formatInt(50000), std::string("50,000"));
    i18n.setLanguage("de");
    CHECK_EQ(i18n.formatInt(50000), std::string("50.000"));
    CHECK_EQ(i18n.formatFloat(2.5, 1), std::string("2,5"));
    i18n.setLanguage("fr");
    CHECK_EQ(i18n.formatInt(50000), "50" + nnbsp + "000");
    i18n.setLanguage("es");
    CHECK_EQ(i18n.formatInt(6000), std::string("6000"));
    CHECK_EQ(i18n.formatInt(12000), std::string("12.000"));
}

TEST(i18n_normalize_tags) {
    CHECK_EQ(I18n::normalizeTag("ru_RU.UTF-8"), std::string("ru"));
    CHECK_EQ(I18n::normalizeTag("zh-Hans-CN"), std::string("zh"));
    CHECK_EQ(I18n::normalizeTag("hi_IN"), std::string("hi"));
    CHECK_EQ(I18n::normalizeTag("de_DE@euro"), std::string("de"));
    CHECK_EQ(I18n::normalizeTag("EN"), std::string("en"));
    CHECK_EQ(I18n::normalizeTag("C"), std::string(""));
    CHECK_EQ(I18n::normalizeTag("POSIX"), std::string(""));
    CHECK_EQ(I18n::normalizeTag("deu"), std::string("de"));
}

#if !defined(_WIN32) && !defined(__APPLE__)
TEST(i18n_detects_system_language) {
    I18n i18n = loadCatalog();
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    setenv("LANGUAGE", "ja_JP:fr_FR:de", 1);
    setenv("LANG", "en_US.UTF-8", 1);
    CHECK_EQ(i18n.detectSystemLanguage(), std::string("fr"));  // ja не поддержан → следующий
    unsetenv("LANGUAGE");
    setenv("LANG", "hi_IN.UTF-8", 1);
    CHECK_EQ(i18n.detectSystemLanguage(), std::string("hi"));
    setenv("LANG", "ja_JP.UTF-8", 1);
    CHECK_EQ(i18n.detectSystemLanguage(), std::string("ru"));  // неподдержанный → основной
    setenv("LANG", "C", 1);
    CHECK_EQ(i18n.detectSystemLanguage(), std::string("ru"));
}
#endif
