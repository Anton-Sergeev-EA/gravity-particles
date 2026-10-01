#include "BloomRenderer.h"

#include <algorithm>

BloomRenderer::BloomRenderer(const std::string& shaderDir, int width, int height, int levels)
    : down_(shaderDir + "/fullscreen.vert", shaderDir + "/bloom_down.frag"),
      up_(shaderDir + "/fullscreen.vert", shaderDir + "/bloom_up.frag"),
      width_(width),
      height_(height),
      levels_(levels) {
    rebuild();
}

void BloomRenderer::rebuild() {
    mips_.clear();
    int w = width_, h = height_;
    for (int i = 0; i < levels_; ++i) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        mips_.push_back(std::make_unique<Framebuffer>(w, h, true));
        if (w == 1 && h == 1) break;
    }
}

void BloomRenderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    rebuild();
}

void BloomRenderer::setLevels(int levels) {
    levels = std::clamp(levels, 2, 9);
    if (levels == levels_) return;
    levels_ = levels;
    rebuild();
}

GLuint BloomRenderer::render(GLuint source, GLuint fullscreenVAO) {
    glBindVertexArray(fullscreenVAO);
    glActiveTexture(GL_TEXTURE0);

    // Вниз по цепочке: каждый уровень — уменьшенная копия предыдущего.
    glDisable(GL_BLEND);
    down_.use();
    down_.setInt("uSource", 0);
    GLuint src = source;
    int srcW = width_, srcH = height_;
    for (size_t i = 0; i < mips_.size(); ++i) {
        mips_[i]->bind();
        down_.setVec2("uTexel", 1.0f / static_cast<float>(srcW), 1.0f / static_cast<float>(srcH));
        down_.setInt("uKaris", i == 0 ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, src);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        src = mips_[i]->colorTexture;
        srcW = mips_[i]->width;
        srcH = mips_[i]->height;
    }

    // Вверх: размытый мелкий уровень аддитивно добавляется к более крупному.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    up_.use();
    up_.setInt("uSource", 0);
    up_.setFloat("uRadius", 1.0f);
    for (size_t i = mips_.size() - 1; i > 0; --i) {
        Framebuffer& from = *mips_[i];
        mips_[i - 1]->bind();
        up_.setVec2("uTexel", 1.0f / static_cast<float>(from.width), 1.0f / static_cast<float>(from.height));
        up_.setFloat("uWeight", 1.0f);
        glBindTexture(GL_TEXTURE_2D, from.colorTexture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glBindVertexArray(0);
    return mips_.front()->colorTexture;
}
