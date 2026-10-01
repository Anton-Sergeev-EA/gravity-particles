#include "Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Разбор чисел не должен зависеть от глобальной локали C (в de_DE «1,5»).
bool parseFloat(const std::string& s, float& out) {
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    float v = 0.0f;
    in >> v;
    if (in.fail()) return false;
    out = v;
    return true;
}

bool parseInt(const std::string& s, int& out) {
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    int v = 0;
    in >> v;
    if (in.fail()) return false;
    out = v;
    return true;
}

}  // namespace

const QualityPreset Settings::kQuality[Settings::kQualityCount] = {
    {60000, 5, 3, 0.34f},     // низкое — встроенная графика, ноутбуки
    {150000, 6, 4, 0.5f},     // среднее
    {400000, 7, 5, 0.5f},     // высокое
    {1000000, 8, 6, 0.75f},   // ультра — дискретные видеокарты
};

int Settings::roundParticles(double value) {
    if (!(value > 0.0)) return kMinParticles;
    value = std::min(value, static_cast<double>(kMaxParticles));  // без переполнения int
    const double magnitude = std::pow(10.0, std::floor(std::log10(value)) - 1.0);
    const double rounded = std::round(value / magnitude) * magnitude;
    return std::clamp(static_cast<int>(rounded), kMinParticles, kMaxParticles);
}

void Settings::clamp() {
    particleCount = roundParticles(std::clamp(particleCount, kMinParticles, kMaxParticles));
    quality = std::clamp(quality, 0, kQualityCount - 1);
    gravity = std::clamp(gravity, 0.1f, 3.0f);
    bloom = std::clamp(bloom, 0.0f, 3.0f);
    particleSize = std::clamp(particleSize, 0.4f, 2.5f);
    trails = std::clamp(trails, 0.0f, 1.0f);
    palette = std::clamp(palette, 0, 3);
}

void Settings::applyQuality(int preset) {
    quality = std::clamp(preset, 0, kQualityCount - 1);
    particleCount = kQuality[quality].particles;
}

void Settings::resetSimulation() {
    Settings defaults;
    quality = defaults.quality;
    particleCount = defaults.particleCount;
    trails = defaults.trails;
    lensing = defaults.lensing;
    cinematic = defaults.cinematic;
    gravity = defaults.gravity;
    bloom = defaults.bloom;
    particleSize = defaults.particleSize;
    palette = defaults.palette;
}

bool Settings::load(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        int i = 0;
        float f = 0.0f;
        if (key == "language") language = value;
        else if (key == "particles" && parseInt(value, i)) particleCount = i;
        else if (key == "gravity" && parseFloat(value, f)) gravity = f;
        else if (key == "bloom" && parseFloat(value, f)) bloom = f;
        else if (key == "particle_size" && parseFloat(value, f)) particleSize = f;
        else if (key == "palette" && parseInt(value, i)) palette = i;
        else if (key == "quality" && parseInt(value, i)) quality = i;
        else if (key == "trails" && parseFloat(value, f)) trails = f;
        else if (key == "lensing" && parseInt(value, i)) lensing = i != 0;
        else if (key == "cinematic" && parseInt(value, i)) cinematic = i != 0;
        else if (key == "panel_visible" && parseInt(value, i)) panelVisible = i != 0;
        else if (key == "help_seen" && parseInt(value, i)) helpSeen = i != 0;
    }
    clamp();
    return true;
}

bool Settings::save(const std::filesystem::path& file) const {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    // Пишем во временный файл и переименовываем — так настройки не
    // повредятся, если программа упадёт посреди записи.
    std::filesystem::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out.imbue(std::locale::classic());
        out << "# Gravity Particles settings\n"
            << "[general]\n"
            << "language = " << language << "\n"
            << "panel_visible = " << (panelVisible ? 1 : 0) << "\n"
            << "help_seen = " << (helpSeen ? 1 : 0) << "\n"
            << "[simulation]\n"
            << "particles = " << particleCount << "\n"
            << "gravity = " << gravity << "\n"
            << "[graphics]\n"
            << "quality = " << quality << "\n"
            << "trails = " << trails << "\n"
            << "lensing = " << (lensing ? 1 : 0) << "\n"
            << "cinematic = " << (cinematic ? 1 : 0) << "\n"
            << "[visual]\n"
            << "bloom = " << bloom << "\n"
            << "particle_size = " << particleSize << "\n"
            << "palette = " << palette << "\n";
        if (!out) return false;
    }
    std::filesystem::rename(tmp, file, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}
