#include "StarField.h"
#include <vector>
#include <cstdlib>

namespace {
float frand(float lo, float hi) {
    return lo + static_cast<float>(rand()) / RAND_MAX * (hi - lo);
}
}

StarField::StarField(int count, float worldW, float worldH) : count_(count) {
    // Каждая звезда: x, y, brightness, twinkleSpeed, phase
    std::vector<float> data;
    data.reserve(count * 5);
    for (int i = 0; i < count; ++i) {
        data.push_back(frand(0, worldW));
        data.push_back(frand(0, worldH));
        data.push_back(frand(0.15f, 0.6f));
        data.push_back(frand(0.5f, 2.5f));
        data.push_back(frand(0.0f, 6.2831853f));
    }

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    const GLsizei stride = 5 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));

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
