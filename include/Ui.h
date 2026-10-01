#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

#include "UiRenderer.h"

struct UiInput {
    float mouseX = -1.0f, mouseY = -1.0f;
    bool mouseDown = false;
    bool mousePressed = false;   // нажатие в этом кадре
    bool mouseReleased = false;  // отпускание в этом кадре
    float scroll = 0.0f;
};

struct UiTheme {
    Color panel = Color::hex(0x0D1120, 0.92f);
    Color panelBorder = Color(1, 1, 1, 0.08f);
    Color text = Color::hex(0xE9EDF7);
    Color muted = Color::hex(0x8F97B8);
    Color accent = Color::hex(0x67E8F9);
    Color accentText = Color::hex(0x061218);
    Color control = Color(1, 1, 1, 0.06f);
    Color controlHover = Color(1, 1, 1, 0.12f);
    Color controlActive = Color(1, 1, 1, 0.18f);
    Color track = Color(1, 1, 1, 0.12f);
};

// Немедленный (immediate-mode) интерфейс: виджеты описываются каждый кадр,
// состояние хранится только для активного элемента (нажатая кнопка,
// перетаскиваемый ползунок). Все размеры задаются в «логических» пикселях
// и умножаются на масштаб экрана (HiDPI).
class Ui {
public:
    explicit Ui(UiRenderer& renderer) : r_(renderer) {}

    void begin(const UiInput& input, float scale, int screenW, int screenH);
    void end();

    UiRenderer& draw() { return r_; }
    TextShaper& shaper() { return r_.shaper(); }
    const UiTheme& theme() const { return theme_; }
    const UiInput& input() const { return in_; }
    float scale() const { return scale_; }
    float s(float logical) const { return logical * scale_; }
    int font(float logical) const;
    int screenW() const { return screenW_; }
    int screenH() const { return screenH_; }

    // Помечает область как принадлежащую интерфейсу: клики в ней не
    // достаются симуляции.
    void block(const Rect& r);
    // Принадлежит ли точка интерфейсу (по раскладке последнего кадра).
    bool wantsMouse(float x, float y) const;
    bool hovered(const Rect& r) const;
    void setInteractive(bool on) { interactive_ = on; }

    void pushClip(const Rect& r);
    void popClip();

    bool button(std::string_view id, const Rect& r, std::string_view label, bool selected = false,
                int fontLogical = 14);
    // Возвращает true, пока значение меняется. released = true в кадре, когда
    // пользователь отпустил ползунок.
    // logarithmic — шкала ползунка логарифмическая (для диапазонов 5 000…1 000 000).
    bool slider(std::string_view id, const Rect& r, std::string_view label,
                std::string_view valueText, float& value, float min, float max, float step,
                bool* released = nullptr, bool logarithmic = false);
    // Переключатель «вкл/выкл» с подписью слева. Возвращает true при изменении.
    bool toggle(std::string_view id, const Rect& r, std::string_view label, bool& value);

private:
    UiRenderer& r_;
    UiTheme theme_;
    UiInput in_;
    float scale_ = 1.0f;
    int screenW_ = 1, screenH_ = 1;
    uint64_t active_ = 0;
    std::vector<Rect> blocks_;
    std::vector<Rect> clips_;
    bool interactive_ = true;

    static uint64_t hashId(std::string_view id);
};
