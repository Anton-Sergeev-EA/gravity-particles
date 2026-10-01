#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

float Vec2::length() const { return std::sqrt(x * x + y * y); }

namespace {
struct ParticleState {
    float x, y;    // позиция
    float vx, vy;  // скорость
};
}  // namespace

ParticleSystem::ParticleSystem(int count, const std::string& updateShaderPath)
    : count_(std::max(1, count)), updateShader_(updateShaderPath, std::vector<std::string>{"oPos", "oVel"}) {
    const GLsizeiptr bytes = static_cast<GLsizeiptr>(count_) * static_cast<GLsizeiptr>(sizeof(ParticleState));
    std::vector<ParticleState> zeros(static_cast<size_t>(count_), ParticleState{0, 0, 0, 0});

    glGenBuffers(2, stateVBO_);
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, stateVBO_[i]);
        glBufferData(GL_ARRAY_BUFFER, bytes, zeros.data(), GL_DYNAMIC_COPY);
    }

    // Единичный квад (-1..1) — из него вершинный шейдер строит вытянутый billboard.
    const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    glGenBuffers(1, &quadVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    const GLsizei stride = sizeof(ParticleState);
    glGenVertexArrays(2, updateVAO_);
    glGenVertexArrays(2, renderVAO_);
    for (int i = 0; i < 2; ++i) {
        // VAO для шага физики: по вершине на частицу.
        glBindVertexArray(updateVAO_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, stateVBO_[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));

        // VAO для отрисовки: квад + состояние частицы как per-instance атрибуты.
        glBindVertexArray(renderVAO_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, stateVBO_[i]);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(1, 1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glVertexAttribDivisor(2, 1);
    }
    glBindVertexArray(0);
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(2, renderVAO_);
    glDeleteVertexArrays(2, updateVAO_);
    glDeleteBuffers(2, stateVBO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
}

void ParticleSystem::update(float dt, const std::vector<Attractor>& attractors, const SimParams& params) {
    ++frame_;
    const int n = std::min(static_cast<int>(attractors.size()), kMaxAttractors);
    std::vector<float> packed(static_cast<size_t>(kMaxAttractors) * 4, 0.0f);
    for (int i = 0; i < n; ++i) {
        const Attractor& a = attractors[static_cast<size_t>(i)];
        packed[static_cast<size_t>(i) * 4 + 0] = a.pos.x;
        packed[static_cast<size_t>(i) * 4 + 1] = a.pos.y;
        packed[static_cast<size_t>(i) * 4 + 2] = a.mass;
        packed[static_cast<size_t>(i) * 4 + 3] = a.horizon;
    }

    updateShader_.use();
    updateShader_.setFloat("uDt", dt);
    updateShader_.setFloat("uGravity", params.gravity);
    // Затухание скорости, не зависящее от частоты кадров (0.998 за кадр при 60 FPS).
    updateShader_.setFloat("uDamping", std::pow(0.998f, dt * 60.0f));
    updateShader_.setVec2("uWorld", params.worldW, params.worldH);
    updateShader_.setInt("uCount", n);
    updateShader_.setVec4Array("uAttr", packed.data(), kMaxAttractors);
    updateShader_.setFloat("uExplode", explodePending_ ? 1.0f : 0.0f);
    updateShader_.setInt("uReset", resetPending_ ? 1 : 0);
    updateShader_.setUInt("uSeed", frame_);
    updateShader_.setInt("uBurstIndex", burstIndex_ < n ? burstIndex_ : -1);
    updateShader_.setFloat("uBurstFraction", burstFraction_);
    burstIndex_ = -1;
    explodePending_ = false;
    resetPending_ = false;

    const int next = 1 - current_;
    glEnable(GL_RASTERIZER_DISCARD);
    glBindVertexArray(updateVAO_[current_]);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, stateVBO_[next]);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, count_);
    glEndTransformFeedback();
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glBindVertexArray(0);
    glDisable(GL_RASTERIZER_DISCARD);
    current_ = next;
}

void ParticleSystem::render() const {
    glBindVertexArray(renderVAO_[current_]);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count_);
    glBindVertexArray(0);
}
