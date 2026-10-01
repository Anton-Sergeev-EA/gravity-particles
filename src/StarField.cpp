#include "StarField.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

// Приближённый цвет звезды по температуре (2 500–12 000 K), нормированный.
void starColor(float kelvin, float& r, float& g, float& b) {
    const float t = kelvin / 100.0f;
    if (t <= 66.0f) {
        r = 1.0f;
        g = std::clamp((99.47f * std::log(t) - 161.12f) / 255.0f, 0.0f, 1.0f);
        b = t <= 19.0f ? 0.0f : std::clamp((138.52f * std::log(t - 10.0f) - 305.04f) / 255.0f, 0.0f, 1.0f);
    } else {
        r = std::clamp(329.70f * std::pow(t - 60.0f, -0.1332f) / 255.0f, 0.0f, 1.0f);
        g = std::clamp(288.12f * std::pow(t - 60.0f, -0.0755f) / 255.0f, 0.0f, 1.0f);
        b = 1.0f;
    }
}

}  // namespace

StarField::StarField(int count, float worldW, float worldH) : count_(count) {
    // Фиксированное зерно: при изменении размера окна узор звёзд стабилен.
    std::mt19937 rng(20251001u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    // Каждая звезда: x, y, размер, яркость, скорость мерцания, фаза, r, g, b
    std::vector<float> data;
    data.reserve(static_cast<size_t>(count) * 9);
    for (int i = 0; i < count; ++i) {
        const float roll = unit(rng);
        float size, brightness;
        if (roll > 0.985f) {  // редкие яркие звёзды с лучами
            size = 12.0f + unit(rng) * 14.0f;
            brightness = 2.2f + unit(rng) * 2.5f;
        } else if (roll > 0.85f) {
            size = 2.5f + unit(rng) * 2.0f;
            brightness = 0.6f + unit(rng) * 0.8f;
        } else {
            size = 1.4f + unit(rng) * 1.2f;
            brightness = 0.15f + unit(rng) * 0.45f;
        }
        // Распределение температур смещено к бело-голубым звёздам.
        const float kelvin = 3200.0f + std::pow(unit(rng), 0.7f) * 9000.0f;
        float r, g, b;
        starColor(kelvin, r, g, b);
        data.insert(data.end(), {unit(rng) * worldW, unit(rng) * worldH, size, brightness,
                                 0.6f + unit(rng) * 2.4f, unit(rng) * 6.2831853f, r, g, b});
    }

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(),
                 GL_STATIC_DRAW);

    const GLsizei stride = 9 * sizeof(float);
    const int sizes[] = {2, 1, 1, 1, 1, 3};
    size_t offset = 0;
    for (GLuint loc = 0; loc < 6; ++loc) {
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, sizes[loc], GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offset * sizeof(float)));
        offset += static_cast<size_t>(sizes[loc]);
    }
    glBindVertexArray(0);
}

StarField::~StarField() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void StarField::render() const {
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, count_);
    glBindVertexArray(0);
}
