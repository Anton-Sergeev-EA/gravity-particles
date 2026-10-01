#pragma once
#include <GL/glew.h>

#include <memory>
#include <string>
#include <vector>

#include "Framebuffer.h"
#include "Shader.h"

// Физически правдоподобный bloom на цепочке уменьшающихся буферов
// (как в Call of Duty: Advanced Warfare и Unreal Engine): изображение
// последовательно уменьшается 13-точечным фильтром, затем увеличивается
// «шатровым» фильтром с накоплением. Получается широкое, мягкое свечение
// без порога яркости и без «квадратных» артефактов.
class BloomRenderer {
public:
    BloomRenderer(const std::string& shaderDir, int width, int height, int levels);
    BloomRenderer(const BloomRenderer&) = delete;
    BloomRenderer& operator=(const BloomRenderer&) = delete;

    void resize(int width, int height);
    void setLevels(int levels);
    // Возвращает текстуру свечения (половинное разрешение).
    GLuint render(GLuint sourceTexture, GLuint fullscreenVAO);
    // Самый мелкий уровень (после render — чистая уменьшенная копия кадра).
    GLuint smallestTexture() const { return mips_.back()->colorTexture; }

private:
    Shader down_, up_;
    std::vector<std::unique_ptr<Framebuffer>> mips_;
    int width_ = 0, height_ = 0, levels_ = 6;
    void rebuild();
};
