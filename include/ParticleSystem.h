#pragma once
#include <GL/glew.h>
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

struct Attractor {
    Vec2 pos;
    float mass = 600.0f;
    bool followsMouse = false;
};

// Данные одной частицы на GPU: позиция, цвет, размер.
// Порядок полей соответствует layout атрибутов в particle.vert.
struct ParticleInstance {
    float x, y;       // позиция
    float r, g, b;    // цвет
    float size;       // радиус визуализации
};

class ParticleSystem {
public:
    explicit ParticleSystem(int count, float worldW, float worldH);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void update(float dt, const std::vector<Attractor>& attractors, float hueShift, int palette);
    void render() const;
    void explode();

private:
    struct CpuParticle {
        Vec2 pos, vel;
        float size;
    };

    std::vector<CpuParticle> particles_;
    std::vector<ParticleInstance> gpuBuffer_;

    GLuint quadVAO_ = 0, quadVBO_ = 0;
    GLuint instanceVBO_ = 0;

    float worldW_, worldH_;

    void setupBuffers();
    static void colorForParticle(float speed, float dist, float hueShift, int palette,
                                  float& r, float& g, float& b);
};
