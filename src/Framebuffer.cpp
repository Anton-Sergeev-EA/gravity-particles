#include "Framebuffer.h"
#include <stdexcept>
#include <iostream>

Framebuffer::Framebuffer(int w, int h, bool floatingPoint) : width(w), height(h), hdr(floatingPoint) {
    create();
}

Framebuffer::~Framebuffer() {
    destroy();
}

void Framebuffer::create() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    // 16-битный float формат позволяет яркости частиц уходить "за пределы" 1.0,
    // что критично для качественного bloom-эффекта.
    GLenum internalFormat = hdr ? GL_RGBA16F : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Framebuffer] Framebuffer не полон!" << std::endl;
        throw std::runtime_error("Ошибка создания framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::destroy() {
    if (colorTexture) glDeleteTextures(1, &colorTexture);
    if (fbo) glDeleteFramebuffers(1, &fbo);
    colorTexture = 0;
    fbo = 0;
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void Framebuffer::unbind(int screenW, int screenH) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
}

void Framebuffer::resize(int w, int h) {
    if (w == width && h == height) return;
    width = w;
    height = h;
    destroy();
    create();
}
