#include "Ui.h"

#include <algorithm>
#include <cmath>

uint64_t Ui::hashId(std::string_view id) {
    uint64_t h = 1469598103934665603ULL;  // FNV-1a
    for (char c : id) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    return h ? h : 1;
}

void Ui::begin(const UiInput& input, float scale, int screenW, int screenH) {
    in_ = input;
    scale_ = scale;
    screenW_ = screenW;
    screenH_ = screenH;
    blocks_.clear();
    clips_.clear();
    r_.begin(screenW, screenH);
}

void Ui::end() {
    if (in_.mouseReleased || !in_.mouseDown) active_ = 0;
    r_.end();
}

int Ui::font(float logical) const {
    return std::max(8, static_cast<int>(std::lround(logical * scale_)));
}

void Ui::block(const Rect& r) {
    blocks_.push_back(r);
}

bool Ui::wantsMouse(float x, float y) const {
    if (active_ != 0) return true;
    for (const Rect& b : blocks_) {
        if (b.contains(x, y)) return true;
    }
    return false;
}

bool Ui::hovered(const Rect& r) const {
    if (!interactive_) return false;
    if (!r.contains(in_.mouseX, in_.mouseY)) return false;
    if (!clips_.empty() && !clips_.back().contains(in_.mouseX, in_.mouseY)) return false;
    return true;
}

void Ui::pushClip(const Rect& r) {
    clips_.push_back(r);
    r_.pushClip(r);
}

void Ui::popClip() {
    if (!clips_.empty()) clips_.pop_back();
    r_.popClip();
}

bool Ui::button(std::string_view id, const Rect& r, std::string_view label, bool selected,
                int fontLogical) {
    const uint64_t hid = hashId(id);
    const bool hover = hovered(r);
    bool clicked = false;
    if (hover && in_.mousePressed) active_ = hid;
    if (active_ == hid && in_.mouseReleased && hover) clicked = true;

    const float radius = s(8.0f);
    Color bg = theme_.control;
    Color fg = theme_.text;
    if (selected) {
        bg = theme_.accent.withAlpha(hover ? 1.0f : 0.92f);
        fg = theme_.accentText;
    } else if (active_ == hid && hover) {
        bg = theme_.controlActive;
    } else if (hover) {
        bg = theme_.controlHover;
    }
    r_.rect(r, bg, radius);
    if (!selected) r_.rectOutline(r, Color(1, 1, 1, hover ? 0.14f : 0.07f), radius, s(1.0f));
    r_.textFit(label, r, font(static_cast<float>(fontLogical)), fg,
               selected ? FontWeight::Bold : FontWeight::Regular, font(10.0f));
    return clicked;
}

bool Ui::slider(std::string_view id, const Rect& r, std::string_view label, std::string_view valueText,
                float& value, float min, float max, float step, bool* released, bool logarithmic) {
    const uint64_t hid = hashId(id);
    const int fpx = font(13.0f);
    const float labelH = shaper().lineHeight(fpx);

    // Подпись слева, значение справа, ниже — дорожка ползунка.
    r_.text(label, r.x, r.y, fpx, theme_.muted);
    r_.text(valueText, r.x + r.w, r.y, fpx, theme_.text, FontWeight::Bold, Align::Right);

    const float trackY = r.y + labelH + s(10.0f);
    const float knobR = s(7.0f);
    const Rect track{r.x + knobR, trackY - s(2.0f), r.w - 2 * knobR, s(4.0f)};
    const Rect hit{r.x, trackY - s(12.0f), r.w, s(24.0f)};

    const bool hover = hovered(hit);
    if (hover && in_.mousePressed) active_ = hid;

    bool changed = false;
    if (active_ == hid && in_.mouseDown) {
        float t = std::clamp((in_.mouseX - track.x) / std::max(1.0f, track.w), 0.0f, 1.0f);
        float v = logarithmic ? min * std::pow(max / min, t) : min + t * (max - min);
        if (step > 0.0f && !logarithmic) v = min + std::round((v - min) / step) * step;
        v = std::clamp(v, min, max);
        if (v != value) {
            value = v;
            changed = true;
        }
    }
    if (released) *released = (active_ == hid && in_.mouseReleased);

    float t = 0.0f;
    if (logarithmic && min > 0.0f && max > min) {
        t = std::clamp(std::log(value / min) / std::log(max / min), 0.0f, 1.0f);
    } else if (max > min) {
        t = std::clamp((value - min) / (max - min), 0.0f, 1.0f);
    }
    r_.rect(track, theme_.track, s(2.0f));
    r_.rect({track.x, track.y, track.w * t, track.h}, theme_.accent, s(2.0f));
    const float kx = track.x + track.w * t;
    const float glow = (active_ == hid) ? 1.0f : (hover ? 0.6f : 0.0f);
    if (glow > 0.0f) {
        r_.rect({kx - knobR * 1.9f, trackY - knobR * 1.9f, knobR * 3.8f, knobR * 3.8f},
                theme_.accent.withAlpha(0.18f * glow), knobR * 1.9f);
    }
    r_.rect({kx - knobR, trackY - knobR, knobR * 2, knobR * 2}, Color::hex(0xF4FBFF), knobR);
    return changed;
}

bool Ui::toggle(std::string_view id, const Rect& r, std::string_view label, bool& value) {
    const uint64_t hid = hashId(id);
    const bool hover = hovered(r);
    if (hover && in_.mousePressed) active_ = hid;
    bool changed = false;
    if (active_ == hid && in_.mouseReleased && hover) {
        value = !value;
        changed = true;
    }

    const float sw = s(38.0f), sh = s(22.0f);
    const Rect sw_{r.x + r.w - sw, r.y + (r.h - sh) * 0.5f, sw, sh};
    const int fpx = font(13.0f);
    // Подпись переносится, если не помещается слева от переключателя.
    const float labelW = r.w - sw - s(12.0f);
    const auto lines = shaper().wrap(label, FontWeight::Regular, fpx, labelW);
    const float lh = shaper().lineHeight(fpx);
    float ty = r.y + (r.h - lh * static_cast<float>(lines.size())) * 0.5f;
    for (const auto& line : lines) {
        r_.text(line, r.x, ty, fpx, hover ? theme_.text : theme_.muted);
        ty += lh;
    }

    r_.rect(sw_, value ? theme_.accent : theme_.track, sh * 0.5f);
    const float knob = sh - s(6.0f);
    const float kx = value ? sw_.x + sw_.w - s(3.0f) - knob : sw_.x + s(3.0f);
    r_.rect({kx, sw_.y + s(3.0f), knob, knob}, value ? theme_.accentText : Color::hex(0xDDE3F0), knob * 0.5f);
    return changed;
}
