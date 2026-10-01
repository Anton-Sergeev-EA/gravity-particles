#pragma once
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "I18n.h"
#include "ParticleSystem.h"
#include "Settings.h"

struct GLFWwindow;
class Shader;
class Framebuffer;
class StarField;
class TextShaper;
class UiRenderer;
class Ui;
class BloomRenderer;
struct UiInput;
struct Rect;

struct LaunchOptions {
    std::string language;   // --lang
    int particles = -1;     // --particles
    bool fullscreen = false;
    int maxFrames = 0;      // --frames: выход после N кадров (для автотестов)
};

// Приложение: окно, ввод, симуляция, рендер и интерфейс.
class App {
public:
    App(std::filesystem::path assetRoot, I18n& i18n, Settings& settings,
        std::filesystem::path settingsFile, const LaunchOptions& options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

    // Обработчики GLFW (вызываются статическими callback-функциями).
    void onMouseButton(int button, int action);
    void onKey(int key, int action);
    void onScroll(double dy);

private:
    struct Toast {
        std::string text;
        float age = 0.0f;
        float duration = 2.6f;
    };

    std::filesystem::path root_;
    I18n& i18n_;
    Settings& settings_;
    std::filesystem::path settingsFile_;
    LaunchOptions options_;

    GLFWwindow* window_ = nullptr;
    int fbW_ = 0, fbH_ = 0;
    float uiScale_ = 1.0f;

    std::unique_ptr<Shader> particleShader_, starShader_, nebulaShader_, copyShader_, lensShader_,
        compositeShader_, adaptShader_;
    std::unique_ptr<ParticleSystem> particles_;
    std::unique_ptr<StarField> stars_;
    std::unique_ptr<BloomRenderer> bloom_;
    // nebula — туманность (пониженное разрешение), trail — накопление шлейфов
    // (ping-pong), scene — фон + частицы, lens — после линзирования.
    std::unique_ptr<Framebuffer> nebulaFB_, trailFB_[2], sceneFB_, lensFB_;
    int trailIndex_ = 0;
    std::unique_ptr<Framebuffer> adaptFB_[2];  // 1×1, адаптация экспозиции (ping-pong)
    int adaptIndex_ = 0;
    std::unique_ptr<TextShaper> shaper_;
    std::unique_ptr<UiRenderer> uiRenderer_;
    std::unique_ptr<Ui> ui_;
    unsigned fsQuadVAO_ = 0, fsQuadVBO_ = 0;

    std::vector<Attractor> attractors_;
    float time_ = 0.0f, hueShift_ = 0.0f, pulse_ = 0.0f;
    bool paused_ = false;
    bool helpOpen_ = false;
    bool dragging_ = false;
    float fps_ = 60.0f;
    float lastDt_ = 1.0f / 60.0f;
    float panelScroll_ = 0.0f, panelContentH_ = 0.0f;
    float freeRight_ = 0.0f;  // правая граница области, не занятой панелью
    float saveTimer_ = -1.0f;

    // Ввод, накопленный callback-функциями между кадрами.
    bool leftDown_ = false, leftPressed_ = false, leftReleased_ = false, rightPressed_ = false;
    float scrollAccum_ = 0.0f;
    std::vector<int> keys_;

    bool fullscreen_ = false;
    int windowedX_ = 100, windowedY_ = 100, windowedW_ = 1280, windowedH_ = 800;

    bool screenshotRequested_ = false;
    std::future<std::string> screenshotJob_;  // путь к файлу или пустая строка при ошибке

    std::vector<Toast> toasts_;

    struct Shockwave {
        Vec2 center;
        float age = 0.0f;
        float strength = 1.0f;
    };
    std::vector<Shockwave> waves_;
    float shake_ = 0.0f;  // сила тряски камеры, затухает

    bool init();
    void shutdown();
    void frame(float dt);
    void handleSimulationInput(float mx, float my);
    void handleKeys();
    void renderScene();
    void captureScreenshot();
    void pollScreenshot();

    void buildUi(const UiInput& input, float dt);
    void drawHud();
    void drawPanel();
    void drawHelp();
    void drawToasts(float dt);

    void setLanguage(const std::string& code, bool announce);
    void cycleLanguage();
    void setPalette(int palette);
    void togglePause();
    void toggleFullscreen();
    void explode();
    void clearSources();
    void resetToDefaults();
    void rebuildParticles();
    void setQuality(int preset);
    void createRenderTargets();
    void addShockwave(Vec2 center, float strength);
    float horizonFor(float mass) const;
    void toast(const std::string& text, float duration = 2.6f);
    void markDirty();
    void updateTitle();
    std::string tr(const char* key) const { return i18n_.tr(key); }
    std::string paletteName(int palette) const;
};
