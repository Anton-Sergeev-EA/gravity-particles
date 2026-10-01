#pragma once
#include <filesystem>
#include <string>

// Пресеты качества графики: сколько частиц и насколько тяжёлая пост-обработка.
struct QualityPreset {
    int particles;
    int bloomLevels;     // глубина цепочки bloom
    int nebulaOctaves;   // детализация туманности
    float nebulaScale;   // разрешение туманности относительно экрана
};

// Пользовательские настройки, сохраняемые между запусками (INI-файл
// в каталоге конфигурации ОС, см. Paths::configDir()).
struct Settings {
    static constexpr int kMinParticles = 5000;
    static constexpr int kMaxParticles = 1000000;
    static constexpr int kQualityCount = 4;
    static const QualityPreset kQuality[kQualityCount];  // низкое, среднее, высокое, ультра

    std::string language;        // пусто — определить по языку системы
    int quality = 2;             // индекс в kQuality
    int particleCount = 400000;
    float gravity = 1.0f;        // множитель силы притяжения
    float bloom = 1.0f;          // сила свечения
    float particleSize = 1.0f;   // множитель размера частиц
    float trails = 0.55f;        // длина шлейфов: 0 — без шлейфов
    bool lensing = true;         // гравитационное линзирование
    bool cinematic = true;       // виньетка, хроматическая аберрация, зерно
    int palette = 0;             // 0 — космос, 1 — неон, 2 — радуга, 3 — пламя
    bool panelVisible = true;
    bool helpSeen = false;       // справка при первом запуске уже показана

    void clamp();
    void applyQuality(int preset);  // выбрать пресет и связанное с ним число частиц
    // Округление до двух значащих цифр: 437 812 → 440 000.
    static int roundParticles(double value);
    void resetSimulation();      // вернуть параметры симуляции/визуализации к умолчаниям

    bool load(const std::filesystem::path& file);
    bool save(const std::filesystem::path& file) const;
};
