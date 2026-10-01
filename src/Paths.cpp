#include "Paths.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#else
#include <climits>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace paths {

namespace {

fs::path homeDir() {
#if defined(_WIN32)
    if (const wchar_t* p = _wgetenv(L"USERPROFILE")) return fs::path(p);
#else
    if (const char* p = std::getenv("HOME")) return fs::path(p);
#endif
    return fs::current_path();
}

// Каталог подходит, только если в нём есть все нужные программе файлы:
// неполная копия (например, папка сборки от старой версии) пропускается.
bool looksLikeAssetRoot(const fs::path& dir) {
    static const char* required[] = {
        "shaders/particle_update.vert", "shaders/particle.vert", "shaders/particle.frag",
        "shaders/star.vert",            "shaders/star.frag",     "shaders/nebula.frag",
        "shaders/copy.frag",            "shaders/lens.frag",     "shaders/bloom_down.frag",
        "shaders/bloom_up.frag",        "shaders/adapt.frag",    "shaders/composite.frag",
        "shaders/fullscreen.vert",      "shaders/ui.vert",       "shaders/ui.frag",
        "locales/languages.json",       "assets/fonts/NotoSans-Regular.ttf",
    };
    std::error_code ec;
    for (const char* rel : required) {
        if (!fs::exists(dir / rel, ec)) return false;
    }
    return true;
}

#if !defined(_WIN32) && !defined(__APPLE__)
// Читает XDG_PICTURES_DIR из ~/.config/user-dirs.dirs.
std::optional<fs::path> xdgPicturesDir() {
    fs::path cfg;
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) cfg = x;
    else cfg = homeDir() / ".config";
    std::ifstream in(cfg / "user-dirs.dirs");
    std::string line;
    while (std::getline(in, line)) {
        const std::string key = "XDG_PICTURES_DIR=";
        if (line.compare(0, key.size(), key) != 0) continue;
        std::string value = line.substr(key.size());
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        const std::string home = "$HOME";
        if (value.compare(0, home.size(), home) == 0) {
            value = homeDir().string() + value.substr(home.size());
        }
        if (!value.empty()) return fs::path(value);
    }
    return std::nullopt;
}
#endif

}  // namespace

fs::path executableDir() {
#if defined(_WIN32)
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return fs::current_path();
        if (n < buf.size()) return fs::path(std::wstring(buf.data(), n)).parent_path();
        buf.resize(buf.size() * 2);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1);
    if (_NSGetExecutablePath(buf.data(), &size) == 0) {
        std::error_code ec;
        fs::path p = fs::canonical(buf.data(), ec);
        if (!ec) return p.parent_path();
    }
    return fs::current_path();
#else
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.parent_path();
    return fs::current_path();
#endif
}

std::optional<fs::path> findAssetRoot() {
    std::vector<fs::path> candidates;
    const fs::path exe = executableDir();
    candidates.push_back(exe);
    candidates.push_back(exe / ".." / "share" / "gravity-particles");
    candidates.push_back(exe / ".." / "Resources");  // macOS .app bundle
    std::error_code ec;
    candidates.push_back(fs::current_path(ec));
#ifdef GP_SOURCE_DIR
    candidates.push_back(fs::path(GP_SOURCE_DIR));
#endif
    for (const auto& c : candidates) {
        if (looksLikeAssetRoot(c)) {
            fs::path canon = fs::weakly_canonical(c, ec);
            return ec ? c : canon;
        }
    }
    return std::nullopt;
}

fs::path configDir() {
#if defined(_WIN32)
    if (const wchar_t* p = _wgetenv(L"APPDATA")) return fs::path(p) / L"GravityParticles";
    return homeDir() / "GravityParticles";
#elif defined(__APPLE__)
    return homeDir() / "Library" / "Application Support" / "GravityParticles";
#else
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) {
        return fs::path(x) / "gravity-particles";
    }
    return homeDir() / ".config" / "gravity-particles";
#endif
}

fs::path screenshotDir() {
    fs::path base;
    std::error_code ec;
#if !defined(_WIN32) && !defined(__APPLE__)
    if (auto xdg = xdgPicturesDir(); xdg && fs::is_directory(*xdg, ec)) base = *xdg;
#endif
    if (base.empty()) {
        fs::path pics = homeDir() / "Pictures";
        base = fs::is_directory(pics, ec) ? pics : homeDir();
    }
    return base / "Gravity Particles";
}

std::string toUtf8(const fs::path& p) {
#if defined(_WIN32)
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr,
                        nullptr);
    return out;
#else
    return p.string();
#endif
}

}  // namespace paths
