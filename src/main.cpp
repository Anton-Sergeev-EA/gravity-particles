// ============================================================
//  Gravity Particles
//  Современный OpenGL 3.3, instanced rendering, bloom,
//  многоязычный интерфейс (FreeType + HarfBuzz)
// ============================================================
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "App.h"
#include "I18n.h"
#include "Paths.h"
#include "Settings.h"
#include "Version.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void printHelp(const I18n& i18n, const std::string& program) {
    std::string codes;
    for (const auto& l : i18n.languages()) codes += (codes.empty() ? "" : ", ") + l.code;
    const std::string minP = std::to_string(Settings::kMinParticles);
    const std::string maxP = std::to_string(Settings::kMaxParticles);

    std::cout << i18n.tr("app.title") << " " << GP_VERSION << " — " << i18n.tr("app.tagline") << "\n\n"
              << i18n.tr("cli.usage", {program}) << "\n\n"
              << "  --lang <code>        " << i18n.tr("cli.lang", {codes}) << "\n"
              << "  --particles <N>      " << i18n.tr("cli.particles", {minP, maxP}) << "\n"
              << "  --fullscreen         " << i18n.tr("cli.fullscreen") << "\n"
              << "  --list-languages     " << i18n.tr("cli.list_languages") << "\n"
              << "  --version            " << i18n.tr("cli.version") << "\n"
              << "  -h, --help           " << i18n.tr("cli.help") << "\n";
}

// Принимает «--key value» и «--key=value».
bool takeValue(const std::vector<std::string>& args, size_t& i, const std::string& key, std::string& out) {
    const std::string& a = args[i];
    if (a == key) {
        if (i + 1 >= args.size()) return false;
        out = args[++i];
        return true;
    }
    if (a.compare(0, key.size() + 1, key + "=") == 0) {
        out = a.substr(key.size() + 1);
        return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);  // вывод справки на хинди/китайском в консоли Windows
#endif
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const auto root = paths::findAssetRoot();
    if (!root) {
        std::cerr << "Не найдены ресурсы программы (папки shaders, locales, assets/fonts).\n"
                  << "Program resources not found (shaders, locales, assets/fonts folders)." << std::endl;
        return 1;
    }

    I18n i18n;
    std::string error;
    if (!i18n.load(*root / "locales", &error)) {
        std::cerr << error << std::endl;
        return 1;
    }

    Settings settings;
    const auto settingsFile = paths::configDir() / "settings.ini";
    settings.load(settingsFile);

    // Язык: сохранённый выбор пользователя → язык системы → русский.
    if (!settings.language.empty() && i18n.isSupported(settings.language)) {
        i18n.setLanguage(settings.language);
    } else {
        i18n.setLanguage(i18n.detectSystemLanguage());
    }

    std::vector<std::string> args(argv + 1, argv + argc);
    const std::string program = argc > 0 ? argv[0] : "gravity_particles";
    LaunchOptions options;

    // --lang применяется первым, чтобы остальные сообщения были на нужном языке.
    for (size_t i = 0; i < args.size(); ++i) {
        std::string value;
        if (takeValue(args, i, "--lang", value)) {
            const std::string code = I18n::normalizeTag(value);
            if (!i18n.isSupported(code)) {
                std::cerr << i18n.tr("cli.unknown_language", {value}) << std::endl;
                return 2;
            }
            i18n.setLanguage(code);
            options.language = code;
        }
    }

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::string value;
        if (a == "-h" || a == "--help") {
            printHelp(i18n, program);
            return 0;
        } else if (a == "--version") {
            std::cout << "gravity_particles " << GP_VERSION << std::endl;
            return 0;
        } else if (a == "--list-languages") {
            for (const auto& l : i18n.languages()) {
                std::cout << l.code << "\t" << l.name << "\t" << l.englishName
                          << (l.code == i18n.defaultLanguage() ? "\t*" : "") << "\n";
            }
            return 0;
        } else if (a == "--fullscreen") {
            options.fullscreen = true;
        } else if (takeValue(args, i, "--lang", value)) {
            // уже обработано выше
        } else if (takeValue(args, i, "--frames", value)) {
            // Служебный параметр для автотестов и CI: отрисовать N кадров и выйти.
            options.maxFrames = std::max(1, std::atoi(value.c_str()));
        } else if (takeValue(args, i, "--particles", value)) {
            char* end = nullptr;
            const long n = std::strtol(value.c_str(), &end, 10);
            if (!end || *end != '\0' || n < Settings::kMinParticles || n > Settings::kMaxParticles) {
                std::cerr << i18n.tr("cli.bad_value", {"--particles", value}) << std::endl;
                return 2;
            }
            options.particles = static_cast<int>(n);
            settings.particleCount = static_cast<int>(n);
            settings.clamp();
        } else {
            std::cerr << i18n.tr("cli.unknown_option", {a}) << "\n\n";
            printHelp(i18n, program);
            return 2;
        }
    }

    try {
        App app(*root, i18n, settings, settingsFile, options);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << i18n.tr("error.fatal", {e.what()}) << std::endl;
        return 1;
    }
}
