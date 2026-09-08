// ============================================================
//  Gravitational Particles — Pro Edition
//  Модерн OpenGL 3.3, instanced rendering, bloom post-processing
// ============================================================
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "Shader.h"
#include "Framebuffer.h"
#include "ParticleSystem.h"
#include "StarField.h"
#include "Math.h"

// -------------------- Конфигурация --------------------
namespace Config {
    int windowW = 1100;
    int windowH = 800;
    int particleCount = 6000;   // современный пайплайн тянет намного больше частиц
    int starCount = 250;
    float bloomStrength = 1.4f;
    int blurPasses = 8;          // чётное число проходов (туда-обратно)
    int blurDownscale = 2;       // размытие считается в уменьшенном разрешении для скорости
}

// -------------------- Состояние приложения --------------------
struct AppState {
    std::vector<Attractor> attractors;
    float hueShift = 0.0f;
    int palette = 0;
    bool mouseDown = false;
    float pulseEnergy = 0.0f;
    float time = 0.0f;
    int windowW = Config::windowW;
    int windowH = Config::windowH;
};

// -------------------- Вспомогательное: полноэкранный квад --------------------
GLuint createFullscreenQuad() {
    const float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    return vao;
}

// -------------------- GLFW callbacks --------------------
void framebufferSizeCallback(GLFWwindow* window, int w, int h) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (w == 0 || h == 0) return;
    state->windowW = w;
    state->windowH = h;
    glViewport(0, 0, w, h);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            state->mouseDown = true;
            Attractor a;
            a.pos = Vec2(static_cast<float>(x), static_cast<float>(y));
            a.mass = 400.0f + static_cast<float>(rand() % 500);
            a.followsMouse = true;
            state->attractors.push_back(a);
        } else {
            state->mouseDown = false;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        state->attractors.clear();
        Attractor center;
        center.pos = Vec2(state->windowW / 2.0f, state->windowH / 2.0f);
        center.mass = 600.0f;
        center.followsMouse = false;
        state->attractors.push_back(center);
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state->mouseDown) return;
    for (auto& a : state->attractors) {
        if (a.followsMouse) {
            a.pos = Vec2(static_cast<float>(x), static_cast<float>(y));
        }
    }
}

ParticleSystem* g_particles = nullptr;

void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_SPACE) {
        state->pulseEnergy = 1.5f;
        if (g_particles) g_particles->explode();
    } else if (key == GLFW_KEY_C) {
        state->palette = (state->palette + 1) % 3;
    }
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // -------------------- Инициализация GLFW / окна --------------------
    if (!glfwInit()) {
        std::cerr << "Не удалось инициализировать GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 0);

    GLFWwindow* window = glfwCreateWindow(Config::windowW, Config::windowH,
                                           "Gravitational Particles — Pro Edition", nullptr, nullptr);
    if (!window) {
        std::cerr << "Не удалось создать окно GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // вертикальная синхронизация

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Не удалось инициализировать GLEW" << std::endl;
        return -1;
    }

    AppState state;
    Attractor center;
    center.pos = Vec2(Config::windowW / 2.0f, Config::windowH / 2.0f);
    center.mass = 600.0f;
    center.followsMouse = false;
    state.attractors.push_back(center);

    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);

    // -------------------- Ресурсы сцены --------------------
    Shader particleShader("shaders/particle.vert", "shaders/particle.frag");
    Shader starShader("shaders/star.vert", "shaders/star.frag");
    Shader blurShader("shaders/fullscreen.vert", "shaders/blur.frag");
    Shader compositeShader("shaders/fullscreen.vert", "shaders/composite.frag");

    ParticleSystem particles(Config::particleCount,
                              static_cast<float>(Config::windowW),
                              static_cast<float>(Config::windowH));
    g_particles = &particles;

    StarField stars(Config::starCount,
                     static_cast<float>(Config::windowW),
                     static_cast<float>(Config::windowH));

    GLuint fsQuad = createFullscreenQuad();

    Framebuffer sceneFB(Config::windowW, Config::windowH, true);
    int blurW = std::max(1, Config::windowW / Config::blurDownscale);
    int blurH = std::max(1, Config::windowH / Config::blurDownscale);
    Framebuffer pingpongFB[2] = {
        Framebuffer(blurW, blurH, true),
        Framebuffer(blurW, blurH, true)
    };

    double lastTime = glfwGetTime();

    // -------------------- Главный цикл --------------------
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        dt = std::min(dt, 0.033f); // защита от скачков при потере фокуса окна
        lastTime = now;
        state.time += dt;

        if (state.pulseEnergy > 0.0f) state.pulseEnergy -= dt * 0.6f;
        if (state.pulseEnergy < 0.0f) state.pulseEnergy = 0.0f;
        state.hueShift += dt * 0.03f;

        // Пересоздаём FBO при изменении размера окна
        if (sceneFB.width != state.windowW || sceneFB.height != state.windowH) {
            sceneFB.resize(state.windowW, state.windowH);
            int bw = std::max(1, state.windowW / Config::blurDownscale);
            int bh = std::max(1, state.windowH / Config::blurDownscale);
            pingpongFB[0].resize(bw, bh);
            pingpongFB[1].resize(bw, bh);
        }

        particles.update(dt, state.attractors, state.hueShift, state.palette);

        float proj[16];
        orthoMatrix(0.0f, static_cast<float>(state.windowW),
                    static_cast<float>(state.windowH), 0.0f, -1.0f, 1.0f, proj);

        // ---------- Проход 1: рендер сцены в HDR framebuffer ----------
        sceneFB.bind();
        glClearColor(0.01f, 0.01f, 0.035f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Звёзды (аддитивно)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        starShader.use();
        starShader.setMat4("uProjection", proj);
        starShader.setFloat("uTime", state.time);
        stars.render();

        // Источники гравитации (яркие ядра, отрисованы как крупные мягкие частицы)
        particleShader.use();
        particleShader.setMat4("uProjection", proj);
        particleShader.setFloat("uSizeMultiplier", 1.0f);
        particleShader.setFloat("uIntensity", 1.0f);

        // Частицы (аддитивное свечение)
        particleShader.setFloat("uSizeMultiplier", 3.5f + state.pulseEnergy * 2.0f);
        particleShader.setFloat("uIntensity", 1.0f + state.pulseEnergy * 0.5f);
        particles.render();

        // ---------- Проход 2: гауссово размытие (bloom) ----------
        bool horizontal = true;
        bool firstIteration = true;
        blurShader.use();
        for (int i = 0; i < Config::blurPasses; ++i) {
            pingpongFB[horizontal].bind();
            blurShader.setInt("uHorizontal", horizontal ? 1 : 0);
            blurShader.setVec2("uTexelSize", 1.0f / pingpongFB[horizontal].width,
                                              1.0f / pingpongFB[horizontal].height);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, firstIteration ? sceneFB.colorTexture
                                                          : pingpongFB[!horizontal].colorTexture);
            blurShader.setInt("uTexture", 0);
            glBindVertexArray(fsQuad);
            glBlendFunc(GL_ONE, GL_ZERO); // без смешивания при размытии
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            horizontal = !horizontal;
            firstIteration = false;
        }

        // ---------- Проход 3: финальный композитинг на экран ----------
        Framebuffer::unbind(state.windowW, state.windowH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlendFunc(GL_ONE, GL_ZERO);

        compositeShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneFB.colorTexture);
        compositeShader.setInt("uScene", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongFB[!horizontal].colorTexture);
        compositeShader.setInt("uBloom", 1);
        compositeShader.setFloat("uBloomStrength", Config::bloomStrength);

        glBindVertexArray(fsQuad);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
