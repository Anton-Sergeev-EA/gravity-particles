#pragma once
#include <GL/glew.h>

#include <string>
#include <vector>

#include "Shader.h"

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const;
};

// Источник гравитации — чёрная дыра.
struct Attractor {
    Vec2 pos;
    float mass = 600.0f;
    float horizon = 10.0f;   // радиус горизонта событий, px
    bool followsMouse = false;
};

struct SimParams {
    float gravity = 1.0f;    // множитель силы притяжения
    float worldW = 0.0f, worldH = 0.0f;
};

// Система частиц, полностью живущая на видеокарте.
//
// Состояние (позиция + скорость) хранится в двух буферах вершин. Каждый кадр
// шейдер particle_update.vert читает буфер A и через transform feedback пишет
// новое состояние в буфер B (без растеризации), затем буферы меняются ролями.
// Тот же буфер сразу служит per-instance данными для отрисовки — данные
// не покидают GPU, поэтому в реальном времени считается до миллиона частиц.
class ParticleSystem {
public:
    static constexpr int kMaxAttractors = 32;

    ParticleSystem(int count, const std::string& updateShaderPath);
    ~ParticleSystem();
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void update(float dt, const std::vector<Attractor>& attractors, const SimParams& params);
    void render() const;     // instanced-отрисовка текущего состояния
    void explode() { explodePending_ = true; }
    void respawnAll() { resetPending_ = true; }
    // Перенести долю частиц на орбиты вокруг источника index (новая галактика).
    void gatherAround(int index, float fraction) {
        burstIndex_ = index;
        burstFraction_ = fraction;
    }
    int count() const { return count_; }
    bool resetPending() const { return resetPending_; }

private:
    int count_;
    Shader updateShader_;
    GLuint stateVBO_[2] = {0, 0};
    GLuint updateVAO_[2] = {0, 0};
    GLuint renderVAO_[2] = {0, 0};
    GLuint quadVBO_ = 0;
    int current_ = 0;
    unsigned frame_ = 0;
    bool explodePending_ = false;
    bool resetPending_ = true;
    int burstIndex_ = -1;
    float burstFraction_ = 0.0f;  // первый кадр раскладывает частицы по орбитам
};
