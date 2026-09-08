#pragma once
#include <GL/glew.h>

// Простой цветной framebuffer с одной текстурой — используется как для сцены,
// так и для промежуточных проходов гауссова размытия (bloom).
class Framebuffer {
public:
    GLuint fbo = 0;
    GLuint colorTexture = 0;
    int width = 0, height = 0;

    Framebuffer(int w, int h, bool floatingPoint = true);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void bind() const;
    static void unbind(int screenW, int screenH);
    void resize(int w, int h);

private:
    bool hdr;
    void create();
    void destroy();
};
