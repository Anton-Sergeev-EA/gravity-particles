#include "App.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <system_error>

#include "BloomRenderer.h"
#include "Framebuffer.h"
#include "Math.h"
#include "Paths.h"
#include "Shader.h"
#include "StarField.h"
#include "TextShaper.h"
#include "Ui.h"
#include "UiRenderer.h"
#include "Version.h"
#include "stb_image_write.h"

namespace fs = std::filesystem;

namespace Config {
constexpr int windowW = 1280;
constexpr int windowH = 800;
constexpr int starCount = 900;
constexpr float centerMass = 600.0f;
constexpr float exposure = 1.15f;
constexpr float exposureKey = 0.03f;  // средняя яркость, выше которой «глаз» прикрывается
constexpr float waveSpeed = 950.0f;    // скорость фронта ударной волны, px/с при высоте 800
constexpr float waveLifetime = 1.4f;
}  // namespace Config

namespace {

App* self(GLFWwindow* w) {
    return static_cast<App*>(glfwGetWindowUserPointer(w));
}

void mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    self(w)->onMouseButton(button, action);
}

void keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    self(w)->onKey(key, action);
}

void scrollCallback(GLFWwindow* w, double /*dx*/, double dy) {
    self(w)->onScroll(dy);
}

std::string shortenHome(const std::string& path) {
#if defined(_WIN32)
    return path;
#else
    const char* home = std::getenv("HOME");
    if (home && *home && path.compare(0, std::string(home).size(), home) == 0) {
        return "~" + path.substr(std::string(home).size());
    }
    return path;
#endif
}

struct PaletteLook {
    float nebulaA[3], nebulaB[3], nebulaC[3];
    float ring[3], glow[3];
};

// Цвета туманности, фотонного кольца и свечения аккреции для каждой палитры.
const PaletteLook& paletteLook(int palette) {
    static const PaletteLook looks[4] = {
        {{0.05f, 0.10f, 0.35f}, {0.30f, 0.12f, 0.55f}, {0.02f, 0.35f, 0.45f}, {1.0f, 0.86f, 0.65f}, {0.30f, 0.55f, 1.0f}},
        {{0.30f, 0.04f, 0.38f}, {0.05f, 0.12f, 0.45f}, {0.45f, 0.05f, 0.25f}, {1.0f, 0.70f, 0.95f}, {0.85f, 0.25f, 1.0f}},
        {{0.08f, 0.18f, 0.40f}, {0.38f, 0.10f, 0.30f}, {0.05f, 0.35f, 0.20f}, {1.0f, 0.95f, 0.85f}, {0.60f, 0.70f, 1.0f}},
        {{0.35f, 0.07f, 0.02f}, {0.12f, 0.04f, 0.18f}, {0.45f, 0.18f, 0.03f}, {1.0f, 0.75f, 0.45f}, {1.0f, 0.42f, 0.12f}},
    };
    return looks[std::clamp(palette, 0, 3)];
}

}  // namespace

App::App(fs::path assetRoot, I18n& i18n, Settings& settings, fs::path settingsFile,
         const LaunchOptions& options)
    : root_(std::move(assetRoot)),
      i18n_(i18n),
      settings_(settings),
      settingsFile_(std::move(settingsFile)),
      options_(options) {}

App::~App() {
    shutdown();
}

// ---------------------------------------------------------------------------
// Инициализация
// ---------------------------------------------------------------------------

bool App::init() {
    if (!glfwInit()) {
        std::cerr << tr("error.glfw_init") << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);  // HiDPI на Windows/Linux

    window_ = glfwCreateWindow(Config::windowW, Config::windowH, tr("app.title").c_str(), nullptr,
                               nullptr);
    if (!window_) {
        std::cerr << tr("error.window") << std::endl;
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // вертикальная синхронизация

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << tr("error.glew") << std::endl;
        return false;
    }
    glGetError();  // GLEW в core-профиле оставляет безвредный GL_INVALID_ENUM

    glfwSetWindowUserPointer(window_, this);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetScrollCallback(window_, scrollCallback);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);

    auto shaderPath = [&](const char* name) { return (root_ / "shaders" / name).string(); };
    particleShader_ = std::make_unique<Shader>(shaderPath("particle.vert"), shaderPath("particle.frag"));
    starShader_ = std::make_unique<Shader>(shaderPath("star.vert"), shaderPath("star.frag"));
    nebulaShader_ = std::make_unique<Shader>(shaderPath("fullscreen.vert"), shaderPath("nebula.frag"));
    copyShader_ = std::make_unique<Shader>(shaderPath("fullscreen.vert"), shaderPath("copy.frag"));
    lensShader_ = std::make_unique<Shader>(shaderPath("fullscreen.vert"), shaderPath("lens.frag"));
    adaptShader_ = std::make_unique<Shader>(shaderPath("fullscreen.vert"), shaderPath("adapt.frag"));
    compositeShader_ =
        std::make_unique<Shader>(shaderPath("fullscreen.vert"), shaderPath("composite.frag"));

    shaper_ = std::make_unique<TextShaper>();
    std::string fontError;
    if (!shaper_->loadDefaultFonts(root_ / "assets" / "fonts", &fontError)) {
        std::cerr << tr("error.assets") << "\n" << fontError << std::endl;
        return false;
    }
    uiRenderer_ = std::make_unique<UiRenderer>(*shaper_, shaderPath("ui.vert"), shaderPath("ui.frag"));
    ui_ = std::make_unique<Ui>(*uiRenderer_);

    glfwGetFramebufferSize(window_, &fbW_, &fbH_);
    fbW_ = std::max(fbW_, 1);
    fbH_ = std::max(fbH_, 1);

    const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    glGenVertexArrays(1, &fsQuadVAO_);
    glBindVertexArray(fsQuadVAO_);
    glGenBuffers(1, &fsQuadVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, fsQuadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    bloom_ = std::make_unique<BloomRenderer>((root_ / "shaders").string(), fbW_, fbH_,
                                             Settings::kQuality[settings_.quality].bloomLevels);
    createRenderTargets();
    for (auto& a : adaptFB_) {
        a = std::make_unique<Framebuffer>(1, 1, true);
        a->bind();
        glClearColor(0, 0, 0, 1);  // 0 — «ещё не адаптировано»
        glClear(GL_COLOR_BUFFER_BIT);
    }

    Attractor center;
    center.pos = Vec2(fbW_ / 2.0f, fbH_ / 2.0f);
    center.mass = Config::centerMass;
    center.horizon = horizonFor(center.mass);
    attractors_.push_back(center);
    rebuildParticles();

    if (options_.fullscreen) toggleFullscreen();
    if (!settings_.helpSeen) helpOpen_ = true;
    toast(tr("toast.welcome"), 5.0f);
    return true;
}

void App::shutdown() {
    if (screenshotJob_.valid()) screenshotJob_.wait();
    if (!window_) return;
    settings_.save(settingsFile_);
    // GL-ресурсы освобождаются, пока контекст ещё жив.
    ui_.reset();
    uiRenderer_.reset();
    particles_.reset();
    stars_.reset();
    bloom_.reset();
    nebulaFB_.reset();
    trailFB_[0].reset();
    trailFB_[1].reset();
    sceneFB_.reset();
    lensFB_.reset();
    particleShader_.reset();
    starShader_.reset();
    nebulaShader_.reset();
    copyShader_.reset();
    lensShader_.reset();
    adaptShader_.reset();
    adaptFB_[0].reset();
    adaptFB_[1].reset();
    compositeShader_.reset();
    if (fsQuadVBO_) glDeleteBuffers(1, &fsQuadVBO_);
    if (fsQuadVAO_) glDeleteVertexArrays(1, &fsQuadVAO_);
    fsQuadVBO_ = fsQuadVAO_ = 0;
    glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
}

int App::run() {
    if (!init()) {
        shutdown();
        glfwTerminate();
        return 1;
    }
    auto last = std::chrono::steady_clock::now();
    int frames = 0;
    while (!glfwWindowShouldClose(window_)) {
        if (options_.maxFrames > 0 && frames++ >= options_.maxFrames) break;
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        frame(dt);
    }
    shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------

void App::onMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftDown_ = true;
            leftPressed_ = true;
        } else if (action == GLFW_RELEASE) {
            leftDown_ = false;
            leftReleased_ = true;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rightPressed_ = true;
    }
}

void App::onKey(int key, int action) {
    if (action == GLFW_PRESS) keys_.push_back(key);
}

void App::onScroll(double dy) {
    scrollAccum_ += static_cast<float>(dy);
}

void App::handleKeys() {
    for (int key : keys_) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                if (helpOpen_) {
                    helpOpen_ = false;
                    settings_.helpSeen = true;
                    markDirty();
                } else {
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                }
                break;
            case GLFW_KEY_SPACE: explode(); break;
            case GLFW_KEY_C: setPalette((settings_.palette + 1) % 4); break;
            case GLFW_KEY_P: togglePause(); break;
            case GLFW_KEY_L: cycleLanguage(); break;
            case GLFW_KEY_TAB:
                settings_.panelVisible = !settings_.panelVisible;
                markDirty();
                break;
            case GLFW_KEY_F1:
                helpOpen_ = !helpOpen_;
                if (!helpOpen_) {
                    settings_.helpSeen = true;
                    markDirty();
                }
                break;
            case GLFW_KEY_F11: toggleFullscreen(); break;
            case GLFW_KEY_F12: screenshotRequested_ = true; break;
            default: break;
        }
    }
    keys_.clear();
}

void App::handleSimulationInput(float mx, float my) {
    // Решение «кому принадлежит клик» принимается по раскладке интерфейса
    // прошлого кадра — она стабильна, а сцена рисуется раньше интерфейса.
    const bool uiOwnsMouse = helpOpen_ || ui_->wantsMouse(mx, my);
    if (leftPressed_ && !uiOwnsMouse) {
        Attractor a;
        a.pos = Vec2(mx, my);
        a.mass = 400.0f + static_cast<float>(std::rand() % 500);
        a.horizon = horizonFor(a.mass);
        a.followsMouse = true;
        // Не больше 32 источников: вытесняется самый старый (кроме центрального).
        if (static_cast<int>(attractors_.size()) >= ParticleSystem::kMaxAttractors) {
            attractors_.erase(attractors_.begin() + 1);
        }
        attractors_.push_back(a);
        particles_->gatherAround(static_cast<int>(attractors_.size()) - 1,
                                 0.6f / static_cast<float>(attractors_.size()));
        addShockwave(a.pos, 0.45f);
        dragging_ = true;
    }
    if (dragging_) {
        for (auto& a : attractors_) {
            if (a.followsMouse) a.pos = Vec2(mx, my);
        }
        if (!leftDown_) {
            dragging_ = false;
            // Отпущенный источник остаётся на месте: иначе следующий захват
            // тащил бы за курсором все ранее созданные источники.
            for (auto& a : attractors_) a.followsMouse = false;
        }
    }
    if (rightPressed_ && !uiOwnsMouse) clearSources();
}

// ---------------------------------------------------------------------------
// Кадр
// ---------------------------------------------------------------------------

void App::frame(float dt) {
    glfwPollEvents();

    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w == 0 || h == 0) {  // окно свёрнуто
        glfwWaitEventsTimeout(0.1);
        return;
    }
    if (dt > 0.0f) fps_ = fps_ * 0.95f + (1.0f / std::max(dt, 1e-4f)) * 0.05f;
    dt = std::min(dt, 0.033f);  // защита от скачков при потере фокуса окна
    // Для автотестов и записи видео: фиксированный шаг 1/60 с независимо от FPS.
    static const bool fixedStep = std::getenv("GP_FIXED_DT") != nullptr;
    if (fixedStep) dt = 1.0f / 60.0f;
    lastDt_ = dt;

    if (w != fbW_ || h != fbH_) {
        fbW_ = w;
        fbH_ = h;
        createRenderTargets();
        bloom_->resize(w, h);
        for (auto& a : attractors_) a.horizon = horizonFor(a.mass);
    }

    float xs = 1.0f, ys = 1.0f;
    glfwGetWindowContentScale(window_, &xs, &ys);
    int winW = 1, winH = 1;
    glfwGetWindowSize(window_, &winW, &winH);
    // На macOS framebuffer в 2 раза больше окна, на Windows/Linux масштаб даёт
    // ContentScale. Интерфейс должен учитывать оба случая.
    const float fbPerWindow = static_cast<float>(fbW_) / static_cast<float>(std::max(winW, 1));
    uiScale_ = std::max(1.0f, std::max(xs, fbPerWindow));

    double cx = 0, cy = 0;
    glfwGetCursorPos(window_, &cx, &cy);
    const float mx = static_cast<float>(cx) * fbPerWindow;
    const float my = static_cast<float>(cy) * fbPerWindow;

    handleKeys();
    handleSimulationInput(mx, my);

    time_ += dt;
    pulse_ = std::max(0.0f, pulse_ - dt * 0.6f);
    shake_ = std::max(0.0f, shake_ - dt * 2.2f);
    for (auto& w : waves_) w.age += dt;
    waves_.erase(std::remove_if(waves_.begin(), waves_.end(),
                                [](const Shockwave& w) { return w.age >= Config::waveLifetime; }),
                 waves_.end());
    if (!paused_ || particles_->resetPending()) {
        if (!paused_) hueShift_ += dt * 0.03f;
        SimParams params;
        params.gravity = settings_.gravity;
        params.worldW = static_cast<float>(fbW_);
        params.worldH = static_cast<float>(fbH_);
        particles_->update(paused_ ? 0.0f : dt, attractors_, params);
    }

    renderScene();
    if (screenshotRequested_) {
        screenshotRequested_ = false;
        captureScreenshot();  // без интерфейса — только сама сцена
    }
    pollScreenshot();

    UiInput input;
    input.mouseX = mx;
    input.mouseY = my;
    input.mouseDown = leftDown_;
    input.mousePressed = leftPressed_;
    input.mouseReleased = leftReleased_;
    input.scroll = scrollAccum_;
    buildUi(input, dt);
    ui_->end();

    leftPressed_ = leftReleased_ = rightPressed_ = false;
    scrollAccum_ = 0.0f;

    if (saveTimer_ >= 0.0f) {
        saveTimer_ -= dt;
        if (saveTimer_ < 0.0f) settings_.save(settingsFile_);
    }

    glfwSwapBuffers(window_);
}

void App::createRenderTargets() {
    const QualityPreset& q = Settings::kQuality[settings_.quality];
    const int nw = std::max(1, static_cast<int>(static_cast<float>(fbW_) * q.nebulaScale));
    const int nh = std::max(1, static_cast<int>(static_cast<float>(fbH_) * q.nebulaScale));
    nebulaFB_ = std::make_unique<Framebuffer>(nw, nh, true);
    for (auto& t : trailFB_) {
        t = std::make_unique<Framebuffer>(fbW_, fbH_, true);
        t->bind();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    sceneFB_ = std::make_unique<Framebuffer>(fbW_, fbH_, true);
    lensFB_ = std::make_unique<Framebuffer>(fbW_, fbH_, true);
    stars_ = std::make_unique<StarField>(Config::starCount, static_cast<float>(fbW_), static_cast<float>(fbH_));
}

float App::horizonFor(float mass) const {
    // Радиус горизонта растёт как √масса и масштабируется с высотой окна.
    return 9.0f * std::sqrt(mass / Config::centerMass) * (static_cast<float>(fbH_) / 800.0f);
}

void App::renderScene() {
    const PaletteLook& look = paletteLook(settings_.palette);
    const QualityPreset& q = Settings::kQuality[settings_.quality];
    const float screenScale = static_cast<float>(fbH_) / 800.0f;
    float proj[16];
    orthoMatrix(0.0f, static_cast<float>(fbW_), static_cast<float>(fbH_), 0.0f, -1.0f, 1.0f, proj);

    glBindVertexArray(fsQuadVAO_);
    glActiveTexture(GL_TEXTURE0);

    // ---------- 1. Туманность (пониженное разрешение) ----------
    glDisable(GL_BLEND);
    nebulaFB_->bind();
    nebulaShader_->use();
    nebulaShader_->setVec2("uResolution", static_cast<float>(nebulaFB_->width),
                           static_cast<float>(nebulaFB_->height));
    nebulaShader_->setFloat("uTime", time_);
    nebulaShader_->setVec3("uColorA", look.nebulaA[0], look.nebulaA[1], look.nebulaA[2]);
    nebulaShader_->setVec3("uColorB", look.nebulaB[0], look.nebulaB[1], look.nebulaB[2]);
    nebulaShader_->setVec3("uColorC", look.nebulaC[0], look.nebulaC[1], look.nebulaC[2]);
    nebulaShader_->setInt("uOctaves", q.nebulaOctaves);
    nebulaShader_->setFloat("uIntensity", 0.42f);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // ---------- 2. Шлейфы: затухание прошлого кадра + новые частицы ----------
    const int prev = trailIndex_;
    trailIndex_ = 1 - trailIndex_;
    Framebuffer& trail = *trailFB_[trailIndex_];
    trail.bind();
    // Длина шлейфа: доля яркости, остающаяся через секунду, не зависит от FPS.
    const float persist = settings_.trails <= 0.001f ? 0.0f : std::pow(settings_.trails, 1.6f) * 0.985f;
    float decay = std::pow(persist, std::max(lastDt_, 1e-4f) * 60.0f);
    if (paused_) decay = persist > 0.0f ? 1.0f : 0.0f;  // на паузе шлейфы «замирают»
    copyShader_->use();
    copyShader_->setInt("uTexture", 0);
    copyShader_->setFloat("uScale", decay);
    glBindTexture(GL_TEXTURE_2D, trailFB_[prev]->colorTexture);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (!paused_ || decay <= 0.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        const float count = static_cast<float>(particles_->count());
        // Чем больше частиц и длиннее шлейф, тем меньше вклад каждой — сцена
        // остаётся сбалансированной по яркости при любых настройках.
        const float density = std::clamp(std::pow(15000.0f / count, 0.75f), 0.02f, 1.6f);
        const float trailNorm = 1.0f - 0.85f * persist;
        particleShader_->use();
        particleShader_->setMat4("uProjection", proj);
        particleShader_->setFloat("uSize", (1.6f + pulse_ * 0.25f) * settings_.particleSize * screenScale *
                                               std::clamp(std::pow(150000.0f / count, 0.18f), 0.75f, 1.6f));
        particleShader_->setFloat("uStretch", 0.018f);
        particleShader_->setFloat("uIntensity", density * trailNorm * (1.0f + pulse_ * 0.15f));
        particleShader_->setFloat("uHueShift", hueShift_);
        particleShader_->setInt("uPalette", settings_.palette);
        particles_->render();
    }

    // ---------- 3. Сцена: туманность + звёзды + шлейфы ----------
    sceneFB_->bind();
    glDisable(GL_BLEND);
    copyShader_->use();
    copyShader_->setFloat("uScale", 1.0f);
    glBindTexture(GL_TEXTURE_2D, nebulaFB_->colorTexture);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    starShader_->use();
    starShader_->setMat4("uProjection", proj);
    starShader_->setFloat("uTime", time_);
    starShader_->setFloat("uScale", std::max(1.0f, screenScale));
    stars_->render();

    copyShader_->use();
    copyShader_->setFloat("uScale", 1.0f);
    glBindTexture(GL_TEXTURE_2D, trail.colorTexture);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // ---------- 4. Чёрные дыры, линзирование, ударные волны ----------
    lensFB_->bind();
    glDisable(GL_BLEND);
    std::vector<float> holes;
    const int holeCount = std::min(static_cast<int>(attractors_.size()), ParticleSystem::kMaxAttractors);
    for (int i = 0; i < holeCount; ++i) {
        const Attractor& a = attractors_[static_cast<size_t>(i)];
        holes.insert(holes.end(), {a.pos.x, static_cast<float>(fbH_) - a.pos.y, a.horizon,
                                   a.mass / Config::centerMass});
    }
    std::vector<float> waves;
    const int waveCount = std::min(static_cast<int>(waves_.size()), 8);
    for (int i = 0; i < waveCount; ++i) {
        const Shockwave& w = waves_[waves_.size() - 1 - static_cast<size_t>(i)];
        const float life = w.age / Config::waveLifetime;
        const float amp = w.strength * (1.0f - life) * (1.0f - life);
        waves.insert(waves.end(), {w.center.x, static_cast<float>(fbH_) - w.center.y,
                                   w.age * Config::waveSpeed * screenScale, amp});
    }
    lensShader_->use();
    lensShader_->setInt("uScene", 0);
    lensShader_->setVec2("uResolution", static_cast<float>(fbW_), static_cast<float>(fbH_));
    lensShader_->setInt("uHoleCount", holeCount);
    lensShader_->setVec4Array("uHoles", holes.data(), holeCount);
    lensShader_->setInt("uWaveCount", waveCount);
    lensShader_->setVec4Array("uWaves", waves.data(), waveCount);
    lensShader_->setFloat("uLensing", settings_.lensing ? 1.0f : 0.0f);
    lensShader_->setVec3("uRingColor", look.ring[0], look.ring[1], look.ring[2]);
    lensShader_->setVec3("uGlowColor", look.glow[0], look.glow[1], look.glow[2]);
    lensShader_->setFloat("uTime", time_);
    glBindTexture(GL_TEXTURE_2D, sceneFB_->colorTexture);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // ---------- 5. Bloom ----------
    const GLuint bloomTex = bloom_->render(lensFB_->colorTexture, fsQuadVAO_);

    // ---------- 5б. Адаптация экспозиции ----------
    const int prevAdapt = adaptIndex_;
    adaptIndex_ = 1 - adaptIndex_;
    adaptFB_[adaptIndex_]->bind();
    glDisable(GL_BLEND);
    adaptShader_->use();
    adaptShader_->setInt("uLuminance", 0);
    adaptShader_->setInt("uPrevious", 1);
    adaptShader_->setFloat("uDt", lastDt_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bloom_->smallestTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, adaptFB_[prevAdapt]->colorTexture);
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glActiveTexture(GL_TEXTURE0);

    if (std::getenv("GP_DEBUG_EXPOSURE")) {
        float px[4] = {};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, px);
        std::cerr << "adapted luminance: " << px[0] << "\n";
    }

    // ---------- 6. Финальный кадр: тонмаппинг и кинематографические эффекты ----------
    Framebuffer::unbind(fbW_, fbH_);
    glDisable(GL_BLEND);
    compositeShader_->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lensFB_->colorTexture);
    compositeShader_->setInt("uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTex);
    compositeShader_->setInt("uBloom", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, adaptFB_[adaptIndex_]->colorTexture);
    compositeShader_->setInt("uAdapted", 2);
    compositeShader_->setFloat("uKey", Config::exposureKey);
    compositeShader_->setFloat("uBloomStrength", settings_.bloom * 0.11f * (1.0f + pulse_ * 1.2f));
    compositeShader_->setFloat("uExposure", Config::exposure);
    compositeShader_->setFloat("uTime", time_);
    compositeShader_->setInt("uCinematic", settings_.cinematic ? 1 : 0);
    compositeShader_->setVec2("uResolution", static_cast<float>(fbW_), static_cast<float>(fbH_));
    const float shakeAmp = settings_.cinematic ? shake_ * shake_ * 0.006f : 0.0f;
    compositeShader_->setVec2("uShake", shakeAmp * std::sin(time_ * 71.0f), shakeAmp * std::cos(time_ * 53.0f));
    glBindVertexArray(fsQuadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// Действия
// ---------------------------------------------------------------------------

void App::rebuildParticles() {
    particles_ = std::make_unique<ParticleSystem>(settings_.particleCount,
                                                  (root_ / "shaders" / "particle_update.vert").string());
    // Старые шлейфы стираются, чтобы не «застыть» поверх новых частиц на паузе.
    for (auto& t : trailFB_) {
        if (!t) continue;
        t->bind();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    Framebuffer::unbind(fbW_, fbH_);
}

void App::setQuality(int preset) {
    settings_.applyQuality(preset);
    bloom_->setLevels(Settings::kQuality[settings_.quality].bloomLevels);
    createRenderTargets();
    rebuildParticles();
    markDirty();
    static const char* keys[] = {"quality.low", "quality.medium", "quality.high", "quality.ultra"};
    toast(i18n_.tr("toast.quality", {i18n_.tr(keys[settings_.quality])}));
}

void App::addShockwave(Vec2 center, float strength) {
    waves_.push_back({center, 0.0f, strength});
    if (waves_.size() > 8) waves_.erase(waves_.begin());
}

void App::toast(const std::string& text, float duration) {
    // Одинаковые сообщения подряд не копятся — обновляется последнее.
    if (!toasts_.empty() && toasts_.back().text == text) {
        toasts_.back().age = 0.0f;
        return;
    }
    toasts_.push_back({text, 0.0f, duration});
    if (toasts_.size() > 4) toasts_.erase(toasts_.begin());
}

void App::markDirty() {
    saveTimer_ = 1.0f;  // отложенное сохранение — не пишем файл на каждый сдвиг ползунка
}

void App::updateTitle() {
    if (window_) glfwSetWindowTitle(window_, tr("app.title").c_str());
}

std::string App::paletteName(int palette) const {
    static const char* keys[] = {"palette.cosmic", "palette.neon", "palette.rainbow", "palette.fire"};
    return i18n_.tr(keys[std::clamp(palette, 0, 3)]);
}

void App::setLanguage(const std::string& code, bool announce) {
    if (!i18n_.setLanguage(code)) return;
    settings_.language = code;
    updateTitle();
    markDirty();
    if (announce) {
        const LanguageInfo* info = i18n_.languageInfo(code);
        toast(i18n_.tr("toast.language", {info ? info->name : code}));
    }
}

void App::cycleLanguage() {
    const auto& langs = i18n_.languages();
    if (langs.empty()) return;
    const size_t next = (static_cast<size_t>(i18n_.languageIndex()) + 1) % langs.size();
    setLanguage(langs[next].code, true);
}

void App::setPalette(int palette) {
    settings_.palette = std::clamp(palette, 0, 3);
    markDirty();
    toast(i18n_.tr("toast.palette", {paletteName(settings_.palette)}));
}

void App::togglePause() {
    paused_ = !paused_;
    toast(tr(paused_ ? "toast.paused" : "toast.resumed"));
}

void App::explode() {
    pulse_ = 1.5f;
    shake_ = 1.0f;
    particles_->explode();
    for (const auto& a : attractors_) addShockwave(a.pos, 1.0f);
}

void App::clearSources() {
    attractors_.clear();
    Attractor center;
    center.pos = Vec2(fbW_ / 2.0f, fbH_ / 2.0f);
    center.mass = Config::centerMass;
    center.horizon = horizonFor(center.mass);
    attractors_.push_back(center);
    addShockwave(center.pos, 0.6f);
    dragging_ = false;
    toast(tr("toast.cleared"));
}

void App::resetToDefaults() {
    const int before = settings_.particleCount;
    const int beforeQuality = settings_.quality;
    settings_.resetSimulation();
    if (settings_.quality != beforeQuality) {
        bloom_->setLevels(Settings::kQuality[settings_.quality].bloomLevels);
        createRenderTargets();
    }
    if (settings_.particleCount != before) rebuildParticles();
    markDirty();
    toast(tr("toast.defaults"));
}

void App::toggleFullscreen() {
    if (!fullscreen_) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedW_, &windowedH_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) return;
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedW_, windowedH_, 0);
    }
    fullscreen_ = !fullscreen_;
    glfwSwapInterval(1);
}

void App::captureScreenshot() {
    const int w = fbW_, h = fbH_;
    std::vector<unsigned char> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    char stamp[32] = {};
    const std::time_t t = std::time(nullptr);
    if (const std::tm* tm = std::localtime(&t)) {
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", tm);
    }
    const fs::path file = paths::screenshotDir() / (std::string("gravity-particles_") + stamp + ".png");

    if (screenshotJob_.valid()) screenshotJob_.wait();
    // Кодирование PNG — в фоновом потоке, чтобы не было рывка кадра.
    screenshotJob_ = std::async(std::launch::async, [pixels = std::move(pixels), w, h, file]() mutable {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        const size_t row = static_cast<size_t>(w) * 4;
        std::vector<unsigned char> flipped(pixels.size());
        for (int y = 0; y < h; ++y) {
            std::copy_n(pixels.data() + static_cast<size_t>(h - 1 - y) * row, row,
                        flipped.data() + static_cast<size_t>(y) * row);
        }
        for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
        const std::string utf8Path = paths::toUtf8(file);
        const int ok = stbi_write_png(utf8Path.c_str(), w, h, 4, flipped.data(), static_cast<int>(row));
        return ok ? utf8Path : std::string();
    });
}

void App::pollScreenshot() {
    if (!screenshotJob_.valid()) return;
    if (screenshotJob_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const std::string path = screenshotJob_.get();
    if (path.empty()) toast(tr("toast.screenshot_failed"), 4.0f);
    else toast(i18n_.tr("toast.screenshot_saved", {shortenHome(path)}), 4.0f);
}

// ---------------------------------------------------------------------------
// Интерфейс
// ---------------------------------------------------------------------------

void App::buildUi(const UiInput& input, float dt) {
    ui_->begin(input, uiScale_, fbW_, fbH_);
    // Пока открыта справка, панель видна, но не реагирует на мышь.
    ui_->setInteractive(!helpOpen_);
    drawHud();
    drawPanel();
    ui_->setInteractive(true);
    drawToasts(dt);  // под окном справки: затемнение перекрывает уведомления
    if (helpOpen_) drawHelp();
}

void App::drawHud() {
    Ui& ui = *ui_;
    UiRenderer& d = ui.draw();
    const UiTheme& th = ui.theme();
    TextShaper& sh = ui.shaper();
    const float m = ui.s(18);

    const int titlePx = ui.font(17);
    const int statPx = ui.font(13);
    float y = m;
    d.text(tr("app.title"), m, y, titlePx, th.text, FontWeight::Bold);
    y += sh.lineHeight(titlePx) + ui.s(2);

    const std::string sep = "   ·   ";
    const std::string stats = tr("hud.fps") + " " + i18n_.formatInt(std::lround(fps_)) + sep +
                              tr("hud.particles") + " " + i18n_.formatInt(particles_->count()) + sep +
                              tr("hud.sources") + " " + i18n_.formatInt(static_cast<long long>(attractors_.size()));
    d.text(stats, m, y, statPx, th.muted);
    y += sh.lineHeight(statPx) + ui.s(8);

    if (paused_) {
        const int px = ui.font(12);
        const std::string label = tr("hud.paused");
        const float w = sh.measure(label, FontWeight::Bold, px) + ui.s(20);
        const Rect badge{m, y, w, sh.lineHeight(px) + ui.s(8)};
        d.rect(badge, th.accent.withAlpha(0.18f), badge.h * 0.5f);
        d.rectOutline(badge, th.accent.withAlpha(0.6f), badge.h * 0.5f, ui.s(1));
        d.text(label, badge.x + badge.w * 0.5f, badge.y + ui.s(4), px, th.accent, FontWeight::Bold,
               Align::Center);
    }

    const int hintPx = ui.font(12);
    d.text(tr("help.hint"), m, static_cast<float>(fbH_) - m - sh.lineHeight(hintPx), hintPx,
           th.muted.withAlpha(0.85f));
}

void App::drawPanel() {
    Ui& ui = *ui_;
    UiRenderer& d = ui.draw();
    const UiTheme& th = ui.theme();
    TextShaper& sh = ui.shaper();
    const float m = ui.s(16);
    freeRight_ = static_cast<float>(fbW_);

    if (!settings_.panelVisible) {
        const std::string label = tr("panel.show");
        const float w = sh.measure(label, FontWeight::Regular, ui.font(13)) + ui.s(32);
        const Rect r{static_cast<float>(fbW_) - w - m, m, w, ui.s(36)};
        ui.block(r);
        d.rect(r, th.panel, ui.s(10));
        if (ui.button("panel.show", r, label, false, 13)) {
            settings_.panelVisible = true;
            markDirty();
        }
        return;
    }

    const float pw = std::min(ui.s(336), static_cast<float>(fbW_) * 0.5f);
    const Rect panel{static_cast<float>(fbW_) - pw - m, m, pw, static_cast<float>(fbH_) - 2 * m};
    freeRight_ = panel.x;
    ui.block(panel);
    d.rect(panel, th.panel, ui.s(16));
    d.rectOutline(panel, th.panelBorder, ui.s(16), ui.s(1));

    const float pad = ui.s(18);
    const float x = panel.x + pad;
    const float w = panel.w - 2 * pad;

    // Заголовок панели (не прокручивается).
    float y = panel.y + pad - ui.s(2);
    const int titlePx = ui.font(18);
    d.text(tr("panel.title"), x, y, titlePx, th.text, FontWeight::Bold);
    const float cb = ui.s(30);
    const Rect closeRect{panel.x + panel.w - pad - cb + ui.s(4), y - ui.s(3), cb, cb};
    if (ui.button("panel.close", closeRect, "×", false, 17)) {
        settings_.panelVisible = false;
        markDirty();
    }
    y += sh.lineHeight(titlePx) + ui.s(10);

    // Прокручиваемая область.
    const Rect area{panel.x, y, panel.w, panel.y + panel.h - y - ui.s(8)};
    if (ui.hovered(area) && ui.input().scroll != 0.0f) panelScroll_ -= ui.input().scroll * ui.s(48);
    panelScroll_ = std::clamp(panelScroll_, 0.0f, std::max(0.0f, panelContentH_ - area.h));
    ui.pushClip(area);

    float cy = area.y - panelScroll_;
    const float contentTop = cy;
    const int sectionPx = ui.font(12);
    auto section = [&](const char* key) {
        cy += ui.s(4);
        d.rect({x, cy + sh.lineHeight(sectionPx) * 0.5f, ui.s(3), ui.s(3)}, th.accent, ui.s(1.5f));
        d.text(tr(key), x + ui.s(10), cy, sectionPx, th.muted, FontWeight::Bold);
        cy += sh.lineHeight(sectionPx) + ui.s(7);
    };
    const float gap = ui.s(7);
    const float bh = ui.s(31);
    const float sliderH = sh.lineHeight(ui.font(13)) + ui.s(22);

    // --- Язык ---
    section("panel.language");
    {
        const auto& langs = i18n_.languages();
        const float bw = (w - gap) * 0.5f;
        for (size_t i = 0; i < langs.size(); ++i) {
            const Rect r{x + static_cast<float>(i % 2) * (bw + gap),
                         cy + static_cast<float>(i / 2) * (bh + gap), bw, bh};
            const bool selected = langs[i].code == i18n_.language();
            if (ui.button("lang." + langs[i].code, r, langs[i].name, selected, 14) && !selected) {
                setLanguage(langs[i].code, true);
            }
        }
        cy += static_cast<float>((langs.size() + 1) / 2) * (bh + gap) + ui.s(6);
    }

    // --- Симуляция ---
    section("panel.simulation");
    {
        float count = static_cast<float>(settings_.particleCount);
        bool released = false;
        if (ui.slider("s.particles", {x, cy, w, sliderH}, tr("panel.particles"),
                      i18n_.formatInt(settings_.particleCount), count,
                      static_cast<float>(Settings::kMinParticles),
                      static_cast<float>(Settings::kMaxParticles), 0.0f, &released, true)) {
            settings_.particleCount = Settings::roundParticles(count);
            markDirty();
        }
        if (released && settings_.particleCount != particles_->count()) {
            rebuildParticles();
            toast(i18n_.tr("toast.particles", {i18n_.formatInt(settings_.particleCount)}));
        }
        cy += sliderH + ui.s(6);

        if (ui.slider("s.gravity", {x, cy, w, sliderH}, tr("panel.gravity"),
                      i18n_.formatFloat(settings_.gravity, 1) + "×", settings_.gravity, 0.1f, 3.0f,
                      0.1f)) {
            markDirty();
        }
        cy += sliderH + ui.s(6);
    }

    // --- Графика ---
    section("panel.graphics");
    {
        const int labelPx = ui.font(13);
        d.text(tr("panel.quality"), x, cy, labelPx, th.muted);
        cy += sh.lineHeight(labelPx) + ui.s(8);
        static const char* qualityKeys[] = {"quality.low", "quality.medium", "quality.high", "quality.ultra"};
        const float qbw = (w - gap) * 0.5f;
        for (int qi = 0; qi < Settings::kQualityCount; ++qi) {
            const Rect r{x + static_cast<float>(qi % 2) * (qbw + gap), cy + static_cast<float>(qi / 2) * (bh + gap),
                         qbw, bh};
            const bool selected = settings_.quality == qi;
            if (ui.button(std::string("quality.") + std::to_string(qi), r, tr(qualityKeys[qi]), selected, 13) &&
                !selected) {
                setQuality(qi);
            }
        }
        cy += 2 * (bh + gap) + ui.s(4);

        if (ui.slider("s.trails", {x, cy, w, sliderH}, tr("panel.trails"),
                      i18n_.formatInt(std::lround(settings_.trails * 100.0f)) + "%", settings_.trails, 0.0f,
                      1.0f, 0.05f)) {
            markDirty();
        }
        cy += sliderH + ui.s(4);
        const float th2 = ui.s(30);
        if (ui.toggle("t.lensing", {x, cy, w, th2}, tr("panel.lensing"), settings_.lensing)) markDirty();
        cy += th2 + ui.s(6);
        if (ui.toggle("t.cinematic", {x, cy, w, th2}, tr("panel.cinematic"), settings_.cinematic)) markDirty();
        cy += th2 + ui.s(10);
    }

    // --- Визуализация ---
    section("panel.visual");
    {
        if (ui.slider("s.bloom", {x, cy, w, sliderH}, tr("panel.bloom"),
                      i18n_.formatFloat(settings_.bloom, 1), settings_.bloom, 0.0f, 3.0f, 0.1f)) {
            markDirty();
        }
        cy += sliderH + ui.s(6);
        if (ui.slider("s.size", {x, cy, w, sliderH}, tr("panel.size"),
                      i18n_.formatFloat(settings_.particleSize, 1) + "×", settings_.particleSize, 0.4f,
                      2.5f, 0.1f)) {
            markDirty();
        }
        cy += sliderH + ui.s(4);

        const int labelPx = ui.font(13);
        d.text(tr("panel.palette"), x, cy, labelPx, th.muted);
        cy += sh.lineHeight(labelPx) + ui.s(8);
        const float pbw = (w - gap) * 0.5f;
        for (int p = 0; p < 4; ++p) {
            const Rect r{x + static_cast<float>(p % 2) * (pbw + gap), cy + static_cast<float>(p / 2) * (bh + gap),
                         pbw, bh};
            const bool selected = settings_.palette == p;
            if (ui.button("palette." + std::to_string(p), r, paletteName(p), selected, 13) && !selected) {
                setPalette(p);
            }
        }
        cy += 2 * (bh + gap) + ui.s(4);
    }

    // --- Действия ---
    section("panel.actions");
    {
        const float bw = (w - gap) * 0.5f;
        auto cell = [&](int i) {
            return Rect{x + static_cast<float>(i % 2) * (bw + gap), cy + static_cast<float>(i / 2) * (bh + gap),
                        bw, bh};
        };
        if (ui.button("a.explode", cell(0), tr("action.explode"), false, 13)) explode();
        if (ui.button("a.clear", cell(1), tr("action.clear"), false, 13)) clearSources();
        if (ui.button("a.pause", cell(2), tr(paused_ ? "action.resume" : "action.pause"), paused_, 13)) {
            togglePause();
        }
        if (ui.button("a.shot", cell(3), tr("action.screenshot"), false, 13)) screenshotRequested_ = true;
        if (ui.button("a.full", cell(4), tr(fullscreen_ ? "action.windowed" : "action.fullscreen"), false,
                      13)) {
            toggleFullscreen();
        }
        if (ui.button("a.help", cell(5), tr("action.help"), helpOpen_, 13)) helpOpen_ = !helpOpen_;
        cy += 3 * (bh + gap);
        if (ui.button("a.defaults", {x, cy, w, bh}, tr("action.defaults"), false, 13)) resetToDefaults();
        cy += bh + ui.s(14);
    }

    // --- Подвал ---
    {
        const int px = ui.font(12);
        d.text(tr("panel.hide_hint"), x, cy, px, th.muted);
        d.text(std::string("v") + GP_VERSION, x + w, cy, px, th.muted.withAlpha(0.7f),
               FontWeight::Regular, Align::Right);
        cy += sh.lineHeight(px) + ui.s(6);
    }

    panelContentH_ = cy - contentTop;
    ui.popClip();

    // Индикатор прокрутки, если содержимое не помещается.
    if (panelContentH_ > area.h + 1.0f) {
        const float ratio = area.h / panelContentH_;
        const float barH = std::max(ui.s(24), area.h * ratio);
        const float t = panelScroll_ / std::max(1.0f, panelContentH_ - area.h);
        d.rect({panel.x + panel.w - ui.s(6), area.y + (area.h - barH) * t, ui.s(3), barH},
               Color(1, 1, 1, 0.22f), ui.s(1.5f));
    }
}

void App::drawHelp() {
    Ui& ui = *ui_;
    UiRenderer& d = ui.draw();
    const UiTheme& th = ui.theme();
    TextShaper& sh = ui.shaper();

    const Rect screen{0, 0, static_cast<float>(fbW_), static_cast<float>(fbH_)};
    ui.block(screen);
    d.rect(screen, Color(0.0f, 0.0f, 0.02f, 0.55f));

    struct Row {
        std::string key, action;
    };
    const std::vector<Row> rows = {
        {tr("key.lmb"), tr("help.add_source")},  {tr("key.lmb_hold"), tr("help.drag_source")},
        {tr("key.rmb"), tr("help.clear")},       {tr("key.space"), tr("help.explode")},
        {"C", tr("help.palette")},               {"P", tr("help.pause")},
        {"Tab", tr("help.panel")},               {"L", tr("help.language")},
        {"F12", tr("help.screenshot")},          {"F11", tr("help.fullscreen")},
        {"F1", tr("help.help")},                 {tr("key.esc"), tr("help.quit")},
    };

    const float cardW = std::min(ui.s(620), static_cast<float>(fbW_) - ui.s(32));
    const float pad = ui.s(28);
    const int titlePx = ui.font(22), tagPx = ui.font(14), keyPx = ui.font(13), rowPx = ui.font(14);
    const float innerW = cardW - 2 * pad;

    float keyColW = 0.0f;
    for (const Row& r : rows) keyColW = std::max(keyColW, sh.measure(r.key, FontWeight::Bold, keyPx));
    keyColW = std::min(keyColW + ui.s(22), innerW * 0.45f);
    const float actionW = innerW - keyColW - ui.s(16);

    // Предварительная раскладка: высоты строк с переносом.
    std::vector<std::vector<std::string>> wrapped;
    float rowsH = 0.0f;
    const float rowLine = sh.lineHeight(rowPx);
    const float rowGap = ui.s(9);
    for (const Row& r : rows) {
        wrapped.push_back(sh.wrap(r.action, FontWeight::Regular, rowPx, actionW));
        rowsH += std::max(rowLine * static_cast<float>(wrapped.back().size()),
                          sh.lineHeight(keyPx) + ui.s(8)) + rowGap;
    }
    const float closeH = ui.s(38);
    const float cardH = pad + sh.lineHeight(titlePx) + ui.s(2) + sh.lineHeight(tagPx) + ui.s(20) + rowsH +
                        ui.s(8) + sh.lineHeight(ui.font(12)) + ui.s(16) + closeH + pad;
    const Rect card{(static_cast<float>(fbW_) - cardW) * 0.5f,
                    std::max(ui.s(16), (static_cast<float>(fbH_) - cardH) * 0.5f), cardW, cardH};

    d.rect(card, Color::hex(0x0D1120, 0.97f), ui.s(18));
    d.rectOutline(card, th.panelBorder, ui.s(18), ui.s(1));

    float y = card.y + pad;
    const float x = card.x + pad;
    d.text(tr("help.title"), x, y, titlePx, th.text, FontWeight::Bold);
    y += sh.lineHeight(titlePx) + ui.s(2);
    d.text(tr("app.title") + " — " + tr("app.tagline"), x, y, tagPx, th.muted);
    y += sh.lineHeight(tagPx) + ui.s(20);

    for (size_t i = 0; i < rows.size(); ++i) {
        const float chipH = sh.lineHeight(keyPx) + ui.s(8);
        const float chipW = std::min(sh.measure(rows[i].key, FontWeight::Bold, keyPx) + ui.s(20), keyColW);
        const Rect chip{x, y, chipW, chipH};
        d.rect(chip, Color(1, 1, 1, 0.07f), ui.s(6));
        d.rectOutline(chip, Color(1, 1, 1, 0.14f), ui.s(6), ui.s(1));
        d.textFit(rows[i].key, chip, keyPx, th.accent, FontWeight::Bold, ui.font(9));

        float ty = y + (chipH - rowLine) * 0.5f;
        for (const std::string& line : wrapped[i]) {
            d.text(line, x + keyColW + ui.s(16), ty, rowPx, th.text);
            ty += rowLine;
        }
        y += std::max(rowLine * static_cast<float>(wrapped[i].size()), chipH) + rowGap;
    }
    y += ui.s(8);
    d.text(tr("help.footer"), x, y, ui.font(12), th.muted);
    y += sh.lineHeight(ui.font(12)) + ui.s(16);

    const float closeW = std::max(ui.s(140), sh.measure(tr("action.close"), FontWeight::Bold, ui.font(14)) + ui.s(40));
    const Rect closeBtn{card.x + card.w - pad - closeW, y, closeW, closeH};
    bool close = ui.button("help.close", closeBtn, tr("action.close"), true, 14);
    // Клик мимо карточки тоже закрывает справку.
    if (ui.input().mousePressed && !card.contains(ui.input().mouseX, ui.input().mouseY)) close = true;
    if (close) {
        helpOpen_ = false;
        settings_.helpSeen = true;
        markDirty();
    }
}

void App::drawToasts(float dt) {
    Ui& ui = *ui_;
    UiRenderer& d = ui.draw();
    TextShaper& sh = ui.shaper();
    const int px = ui.font(14);
    const float h = sh.lineHeight(px) + ui.s(18);
    float y = static_cast<float>(fbH_) - ui.s(56) - h;

    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        it->age += dt;
        const float fadeIn = std::clamp(it->age / 0.18f, 0.0f, 1.0f);
        const float fadeOut = std::clamp((it->duration - it->age) / 0.45f, 0.0f, 1.0f);
        const float a = std::min(fadeIn, fadeOut);
        if (a <= 0.0f) continue;
        const float tw = sh.measure(it->text, FontWeight::Regular, px);
        // Центрируем в свободной от панели области, если уведомление там помещается.
        const float areaW = (tw + ui.s(72) <= freeRight_) ? freeRight_ : static_cast<float>(fbW_);
        const float w = std::min(tw + ui.s(40), areaW - ui.s(32));
        const Rect r{(areaW - w) * 0.5f, y + (1.0f - fadeIn) * ui.s(8), w, h};
        d.rect(r, Color::hex(0x101528, 0.92f * a), h * 0.5f);
        d.rectOutline(r, ui.theme().accent.withAlpha(0.35f * a), h * 0.5f, ui.s(1));
        d.text(it->text, r.x + r.w * 0.5f, r.y + ui.s(9), px, ui.theme().text.withAlpha(a),
               FontWeight::Regular, Align::Center);
        y -= h + ui.s(8);
    }
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [](const Toast& t) { return t.age >= t.duration; }),
                  toasts_.end());
}
