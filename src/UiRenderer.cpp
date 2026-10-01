#include "UiRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Math.h"

UiRenderer::UiRenderer(TextShaper& shaper, const std::string& vertPath, const std::string& fragPath)
    : shaper_(shaper), shader_(vertPath, fragPath) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, lx));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, radius));
    glBindVertexArray(0);

    glGenTextures(1, &atlas_);
    resetAtlas(atlasSize_);
}

UiRenderer::~UiRenderer() {
    if (atlas_) glDeleteTextures(1, &atlas_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void UiRenderer::resetAtlas(int size) {
    atlasSize_ = size;
    glyphs_.clear();
    shelfX_ = shelfY_ = 1;
    shelfH_ = 0;
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    std::vector<uint8_t> zeros(static_cast<size_t>(size) * static_cast<size_t>(size), 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, zeros.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool UiRenderer::allocate(int w, int h, int& x, int& y) {
    // Простая «полочная» упаковка: глифы одного кегля близки по высоте.
    if (shelfX_ + w + 1 > atlasSize_) {
        shelfX_ = 1;
        shelfY_ += shelfH_ + 1;
        shelfH_ = 0;
    }
    if (shelfY_ + h + 1 > atlasSize_) return false;
    x = shelfX_;
    y = shelfY_;
    shelfX_ += w + 1;
    shelfH_ = std::max(shelfH_, h);
    return true;
}

const UiRenderer::GlyphSlot* UiRenderer::glyph(uint16_t face, uint32_t glyphId, int px) {
    const uint64_t key = (static_cast<uint64_t>(face) << 48) | (static_cast<uint64_t>(glyphId) << 16) |
                         static_cast<uint64_t>(px & 0xFFFF);
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) return &it->second;

    GlyphBitmap bm;
    if (!shaper_.rasterize(face, glyphId, px, bm)) return nullptr;

    GlyphSlot slot{};
    slot.w = bm.width;
    slot.h = bm.height;
    slot.left = bm.left;
    slot.top = bm.top;
    if (bm.width > 0 && bm.height > 0) {
        int x = 0, y = 0;
        if (!allocate(bm.width, bm.height, x, y)) {
            // Атлас заполнен: дорисовываем то, что уже в очереди, и начинаем
            // заново — при необходимости вдвое большего размера.
            flush();
            resetAtlas(std::min(atlasSize_ * 2, 4096));
            if (!allocate(bm.width, bm.height, x, y)) return nullptr;
        }
        glBindTexture(GL_TEXTURE_2D, atlas_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bm.width, bm.height, GL_RED, GL_UNSIGNED_BYTE,
                        bm.pixels.data());
        const float inv = 1.0f / static_cast<float>(atlasSize_);
        slot.u0 = static_cast<float>(x) * inv;
        slot.v0 = static_cast<float>(y) * inv;
        slot.u1 = static_cast<float>(x + bm.width) * inv;
        slot.v1 = static_cast<float>(y + bm.height) * inv;
    }
    return &glyphs_.emplace(key, slot).first->second;
}

void UiRenderer::begin(int screenW, int screenH) {
    screenW_ = std::max(1, screenW);
    screenH_ = std::max(1, screenH);
    vertices_.clear();
    clipStack_.clear();
    glDisable(GL_SCISSOR_TEST);
}

void UiRenderer::end() {
    flush();
    glDisable(GL_SCISSOR_TEST);
}

void UiRenderer::flush() {
    if (vertices_.empty()) return;
    float proj[16];
    orthoMatrix(0.0f, static_cast<float>(screenW_), static_cast<float>(screenH_), 0.0f, -1.0f, 1.0f,
                proj);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    shader_.use();
    shader_.setMat4("uProjection", proj);
    shader_.setInt("uAtlas", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    vertices_.clear();
}

void UiRenderer::applyClip() {
    if (clipStack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect& c = clipStack_.back();
    glEnable(GL_SCISSOR_TEST);
    int x = static_cast<int>(std::floor(c.x));
    int y = static_cast<int>(std::floor(static_cast<float>(screenH_) - (c.y + c.h)));
    glScissor(x, y, std::max(0, static_cast<int>(std::ceil(c.w))),
              std::max(0, static_cast<int>(std::ceil(c.h))));
}

void UiRenderer::pushClip(const Rect& r) {
    flush();
    Rect c = r;
    if (!clipStack_.empty()) {
        const Rect& p = clipStack_.back();
        float x0 = std::max(c.x, p.x), y0 = std::max(c.y, p.y);
        float x1 = std::min(c.x + c.w, p.x + p.w), y1 = std::min(c.y + c.h, p.y + p.h);
        c = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
    }
    clipStack_.push_back(c);
    applyClip();
}

void UiRenderer::popClip() {
    flush();
    if (!clipStack_.empty()) clipStack_.pop_back();
    applyClip();
}

void UiRenderer::quad(const Rect& r, Color top, Color bottom, float radius, float mode,
                      float border) {
    // Расширяем квад на 1 px, чтобы сглаживание края SDF не обрезалось.
    const float pad = 1.0f;
    const float hw = r.w * 0.5f, hh = r.h * 0.5f;
    const float x0 = r.x - pad, y0 = r.y - pad, x1 = r.x + r.w + pad, y1 = r.y + r.h + pad;
    radius = std::min(radius, std::min(hw, hh));
    auto v = [&](float x, float y, Color c) {
        Vertex vx{};
        vx.x = x;
        vx.y = y;
        vx.r = c.r;
        vx.g = c.g;
        vx.b = c.b;
        vx.a = c.a;
        vx.lx = x - (r.x + hw);
        vx.ly = y - (r.y + hh);
        vx.hw = hw;
        vx.hh = hh;
        vx.radius = radius;
        vx.mode = mode;
        vx.border = border;
        return vx;
    };
    const Vertex a = v(x0, y0, top), b = v(x1, y0, top), c = v(x1, y1, bottom), d = v(x0, y1, bottom);
    vertices_.insert(vertices_.end(), {a, b, c, a, c, d});
}

void UiRenderer::rect(const Rect& r, Color c, float radius) {
    quad(r, c, c, radius, 1.0f, 0.0f);
}

void UiRenderer::verticalGradient(const Rect& r, Color top, Color bottom, float radius) {
    quad(r, top, bottom, radius, 1.0f, 0.0f);
}

void UiRenderer::rectOutline(const Rect& r, Color c, float radius, float thickness) {
    quad(r, c, c, radius, 2.0f, thickness);
}

float UiRenderer::text(std::string_view s, float x, float y, int px, Color c, FontWeight w,
                       Align align) {
    if (s.empty()) return 0.0f;
    const ShapedLine& line = shaper_.shape(s, w, px);
    if (align == Align::Center) x -= line.width * 0.5f;
    else if (align == Align::Right) x -= line.width;
    // Привязка базовой линии к целому пикселю — текст остаётся резким.
    const float baseline = std::round(y + shaper_.ascender(px));
    x = std::round(x);

    for (const ShapedGlyph& g : line.glyphs) {
        const GlyphSlot* slot = glyph(g.face, g.glyph, px);
        if (!slot || slot->w == 0) continue;
        const float gx = std::round(x + g.x) + static_cast<float>(slot->left);
        const float gy = std::round(baseline + g.y) - static_cast<float>(slot->top);
        const float gw = static_cast<float>(slot->w), gh = static_cast<float>(slot->h);
        auto v = [&](float vx, float vy, float u, float vv) {
            Vertex out{};
            out.x = vx;
            out.y = vy;
            out.u = u;
            out.v = vv;
            out.r = c.r;
            out.g = c.g;
            out.b = c.b;
            out.a = c.a;
            out.mode = 0.0f;
            return out;
        };
        const Vertex a = v(gx, gy, slot->u0, slot->v0), b = v(gx + gw, gy, slot->u1, slot->v0),
                     cc = v(gx + gw, gy + gh, slot->u1, slot->v1), d = v(gx, gy + gh, slot->u0, slot->v1);
        vertices_.insert(vertices_.end(), {a, b, cc, a, cc, d});
    }
    return line.width;
}

void UiRenderer::textFit(std::string_view s, const Rect& r, int px, Color c, FontWeight w, int minPx) {
    float width = shaper_.measure(s, w, px);
    const float avail = r.w - 12.0f;
    while (width > avail && px > minPx) {
        --px;
        width = shaper_.measure(s, w, px);
    }
    const float y = r.y + (r.h - shaper_.lineHeight(px)) * 0.5f;
    text(s, r.x + r.w * 0.5f, y, px, c, w, Align::Center);
}
