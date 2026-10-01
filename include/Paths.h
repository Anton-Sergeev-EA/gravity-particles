#pragma once
#include <filesystem>
#include <optional>
#include <string>

// Кроссплатформенные пути: каталог программы, ресурсы, конфигурация, картинки.
namespace paths {

std::filesystem::path executableDir();

// Ищет каталог ресурсов (shaders/, locales/, assets/fonts/) рядом с
// исполняемым файлом, в ../share/gravity-particles (установка через
// `cmake --install`), в текущем каталоге и в каталоге исходников.
std::optional<std::filesystem::path> findAssetRoot();

// ~/.config/gravity-particles (Linux), ~/Library/Application Support/
// GravityParticles (macOS), %APPDATA%\GravityParticles (Windows).
std::filesystem::path configDir();

// Каталог для скриншотов: XDG_PICTURES_DIR («Изображения» в русской
// локали Ubuntu), ~/Pictures или домашний каталог + "Gravity Particles".
std::filesystem::path screenshotDir();

// UTF-8 строка пути для вывода в интерфейсе.
std::string toUtf8(const std::filesystem::path& p);

}  // namespace paths
