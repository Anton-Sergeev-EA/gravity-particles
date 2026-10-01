#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct LanguageInfo {
    std::string code;         // "ru", "en", "zh", ...
    std::string name;         // самоназвание: "Русский", "中文", "हिन्दी"
    std::string englishName;  // "Russian", "Chinese (Simplified)"
};

// Каталог переводов.
//
// * Манифест locales/languages.json задаёт порядок языков, язык по умолчанию
//   и резервный язык (из него берутся строки, отсутствующие в текущем).
// * Каждый locales/<code>.json — вложенный JSON-объект, ключи разворачиваются
//   в плоские «a.b.c».
// * Подстановки: "{0}", "{1}" … — позиционные аргументы tr().
// * Числа форматируются по правилам языка (разделитель групп, десятичный
//   разделитель, группировка «3» или индийская «3;2»: 1,00,000).
class I18n {
public:
    // Загружает манифест и все перечисленные в нём локали.
    // При ошибке возвращает false и пишет причину в error.
    bool load(const std::filesystem::path& localesDir, std::string* error = nullptr);

    const std::vector<LanguageInfo>& languages() const { return languages_; }
    const std::string& defaultLanguage() const { return default_; }
    const std::string& fallbackLanguage() const { return fallback_; }

    bool isSupported(std::string_view code) const;
    bool setLanguage(std::string_view code);
    const std::string& language() const { return current_; }
    int languageIndex() const;
    const LanguageInfo* languageInfo(std::string_view code) const;

    // Перевод по ключу: текущий язык → резервный → сам ключ.
    std::string tr(std::string_view key) const;
    std::string tr(std::string_view key, const std::vector<std::string>& args) const;

    std::string formatInt(long long value) const;
    std::string formatFloat(double value, int decimals) const;

    // Для тестов и инструментов.
    bool has(std::string_view language, std::string_view key) const;
    std::vector<std::string> keys(std::string_view language) const;
    const std::string* raw(std::string_view language, std::string_view key) const;

    // "ru_RU.UTF-8" → "ru", "zh-Hans-CN" → "zh", "C"/"POSIX"/"" → "".
    static std::string normalizeTag(std::string_view tag);
    static std::string format(std::string_view pattern, const std::vector<std::string>& args);

    // Язык системы, если он поддерживается; иначе язык по умолчанию.
    std::string detectSystemLanguage() const;
    // Список предпочтительных языков ОС (сырые теги, по убыванию приоритета).
    static std::vector<std::string> systemLanguageTags();

private:
    using Table = std::unordered_map<std::string, std::string>;
    std::vector<LanguageInfo> languages_;
    std::unordered_map<std::string, Table> tables_;
    std::string default_ = "ru";
    std::string fallback_ = "ru";
    std::string current_ = "ru";
};
