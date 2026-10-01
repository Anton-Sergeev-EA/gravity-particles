#pragma once
#include <GL/glew.h>

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Shader.h"
#include "TextShaper.h"

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    static constexpr Color hex(uint32_t rgb, float a = 1.0f) {
        return Color(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                     static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                     static_cast<float>(rgb & 0xFF) / 255.0f, a);
    }
    Color withAlpha(float alpha) const { return Color(r, g, b, a * alpha); }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

enum class Align { Left, Center, Right };

// Пакетный рендерер 2D-интерфейса: скруглённые прямоугольники (SDF со
// сглаживанием) и текст из динамического атласа глифов. Весь интерфейс
// кадра рисуется одним-несколькими draw call.
class UiRenderer {
public:
    UiRenderer(TextShaper& shaper, const std::string& vertPath, const std::string& fragPath);
    ~UiRenderer();
    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    void begin(int screenW, int screenH);
    void end();  // отправляет накопленную геометрию на GPU

    void rect(const Rect& r, Color c, float radius = 0.0f);
    void rectOutline(const Rect& r, Color c, float radius, float thickness);
    void verticalGradient(const Rect& r, Color top, Color bottom, float radius = 0.0f);

    // y — верх строки. Возвращает ширину текста.
    float text(std::string_view s, float x, float y, int px, Color c,
               FontWeight w = FontWeight::Regular, Align align = Align::Left);
    // Текст, вписанный в прямоугольник по центру; при нехватке ширины кегль уменьшается.
    void textFit(std::string_view s, const Rect& r, int px, Color c,
                 FontWeight w = FontWeight::Regular, int minPx = 10);

    void pushClip(const Rect& r);
    void popClip();

    TextShaper& shaper() { return shaper_; }

private:
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
        float lx, ly, hw, hh;  // локальные координаты в прямоугольнике и его полуразмеры
        float radius;
        float mode;    // 0 — глиф, 1 — заливка, 2 — обводка
        float border;
    };
    struct GlyphSlot {
        float u0, v0, u1, v1;
        int w, h, left, top;
    };

    TextShaper& shaper_;
    Shader shader_;
    GLuint vao_ = 0, vbo_ = 0, atlas_ = 0;
    int atlasSize_ = 1024;
    int shelfX_ = 1, shelfY_ = 1, shelfH_ = 0;
    std::unordered_map<uint64_t, GlyphSlot> glyphs_;
    std::vector<Vertex> vertices_;
    std::vector<Rect> clipStack_;
    int screenW_ = 1, screenH_ = 1;

    const GlyphSlot* glyph(uint16_t face, uint32_t glyph, int px);
    bool allocate(int w, int h, int& x, int& y);
    void resetAtlas(int size);
    void flush();
    void applyClip();
    void quad(const Rect& r, Color top, Color bottom, float radius, float mode, float border);
};
