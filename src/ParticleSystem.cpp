#include "ParticleSystem.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

float Vec2::length() const { return std::sqrt(x * x + y * y); }

namespace {
float frand(float lo, float hi) {
    return lo + static_cast<float>(rand()) / RAND_MAX * (hi - lo);
}

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = std::fmod(h, 1.0f);
    if (h < 0) h += 1.0f;
    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}
} // namespace

ParticleSystem::ParticleSystem(int count, float worldW, float worldH)
    : worldW_(worldW), worldH_(worldH) {
    particles_.reserve(count);
    gpuBuffer_.resize(count);

    for (int i = 0; i < count; ++i) {
        CpuParticle p;
        float angle = frand(0.0f, 6.2831853f);
        float radius = frand(50.0f, worldH * 0.4f);
        p.pos = Vec2(worldW / 2.0f + std::cos(angle) * radius,
                     worldH / 2.0f + std::sin(angle) * radius);
        p.vel = Vec2(frand(-20.0f, 20.0f), frand(-20.0f, 20.0f));
        p.size = frand(2.0f, 4.5f);
        particles_.push_back(p);
    }

    setupBuffers();
}

ParticleSystem::~ParticleSystem() {
    if (instanceVBO_) glDeleteBuffers(1, &instanceVBO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
}

void ParticleSystem::setupBuffers() {
    // Единичный квад (-1..1), из которого фрагментный шейдер вырезает мягкий круг.
    const float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays(1, &quadVAO_);
    glBindVertexArray(quadVAO_);

    glGenBuffers(1, &quadVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenBuffers(1, &instanceVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, gpuBuffer_.size() * sizeof(ParticleInstance),
                 nullptr, GL_DYNAMIC_DRAW);

    // location 1: позиция частицы (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, x));
    glVertexAttribDivisor(1, 1);

    // location 2: цвет частицы (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, r));
    glVertexAttribDivisor(2, 1);

    // location 3: размер частицы (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, size));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void ParticleSystem::colorForParticle(float speed, float dist, float hueShift, int palette,
                                       float& r, float& g, float& b) {
    float hue;
    switch (palette) {
        case 0: // огненная палитра
            hue = 0.60f - speed * 0.0022f + hueShift;
            hsvToRgb(hue, 0.85f, 1.0f, r, g, b);
            break;
        case 1: // неоново-фиолетовая
            hue = 0.78f + std::sin(dist * 0.01f) * 0.15f + hueShift * 0.5f;
            hsvToRgb(hue, 0.9f, 1.0f, r, g, b);
            break;
        default: // радужная
            hue = dist * 0.002f + hueShift * 1.5f;
            hsvToRgb(hue, 0.8f, 1.0f, r, g, b);
            break;
    }
}

void ParticleSystem::update(float dt, const std::vector<Attractor>& attractors,
                             float hueShift, int palette) {
    for (size_t idx = 0; idx < particles_.size(); ++idx) {
        auto& p = particles_[idx];
        Vec2 force(0.0f, 0.0f);
        float nearestDist = 1e9f;

        for (auto& a : attractors) {
            Vec2 dir = a.pos - p.pos;
            float distSq = dir.x * dir.x + dir.y * dir.y + 400.0f;
            float dist = std::sqrt(distSq);
            if (dist < nearestDist) nearestDist = dist;
            float forceMag = (900.0f * a.mass) / distSq;
            force = force + Vec2(dir.x / dist, dir.y / dist) * forceMag;
        }

        p.vel = (p.vel + force * dt) * 0.998f;
        p.pos = p.pos + p.vel * dt;

        float speed = p.vel.length();
        float r, g, b;
        colorForParticle(speed, nearestDist, hueShift, palette, r, g, b);

        if (p.pos.x < -100 || p.pos.x > worldW_ + 100 ||
            p.pos.y < -100 || p.pos.y > worldH_ + 100) {
            float angle = frand(0.0f, 6.2831853f);
            p.pos = Vec2(worldW_ / 2.0f + std::cos(angle) * 20.0f,
                         worldH_ / 2.0f + std::sin(angle) * 20.0f);
            p.vel = Vec2(frand(-10.0f, 10.0f), frand(-10.0f, 10.0f));
        }

        ParticleInstance& gp = gpuBuffer_[idx];
        gp.x = p.pos.x;
        gp.y = p.pos.y;
        gp.r = r; gp.g = g; gp.b = b;
        gp.size = p.size;
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gpuBuffer_.size() * sizeof(ParticleInstance), gpuBuffer_.data());
}

void ParticleSystem::render() const {
    glBindVertexArray(quadVAO_);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(gpuBuffer_.size()));
    glBindVertexArray(0);
}

void ParticleSystem::explode() {
    for (auto& p : particles_) {
        float angle = frand(0.0f, 6.2831853f);
        float speed = frand(250.0f, 600.0f);
        p.vel = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);
    }
}
