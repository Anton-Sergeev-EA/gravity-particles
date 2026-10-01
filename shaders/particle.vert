#version 330 core
// Частица — billboard, вытянутый вдоль скорости (motion blur): быстрые частицы
// превращаются в светящиеся штрихи. Цвет зависит от скорости («температуры»).

layout(location = 0) in vec2 aCorner;  // угол базового квада (-1..1)
layout(location = 1) in vec2 iPos;     // позиция частицы
layout(location = 2) in vec2 iVel;     // скорость частицы

uniform mat4 uProjection;
uniform float uSize;
uniform float uStretch;
uniform float uIntensity;
uniform float uHueShift;
uniform int uPalette;

out vec2 vUV;
out vec3 vColor;

uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

vec3 hsv2rgb(vec3 c) {
    vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

vec3 ramp(vec3 a, vec3 b, vec3 c, float t) {
    return t < 0.5 ? mix(a, b, t * 2.0) : mix(b, c, (t - 0.5) * 2.0);
}

vec3 palette(float t, float h) {
    if (uPalette == 0) {  // космос: глубокий синий → бирюза → бело-голубой
        return ramp(vec3(0.10, 0.22, 1.00), vec3(0.10, 0.80, 1.00), vec3(0.55, 0.88, 1.00), t);
    } else if (uPalette == 1) {  // неон: фиолетовый → маджента → розово-белый
        return ramp(vec3(0.45, 0.10, 1.00), vec3(1.00, 0.12, 0.70), vec3(1.00, 0.55, 0.90), t);
    } else if (uPalette == 2) {  // радуга
        // оттенок задаётся направлением движения: вращающийся диск
        // превращается в спектральное колесо
        return hsv2rgb(vec3(fract(atan(iVel.y, iVel.x) * 0.15915 + t * 0.3 + uHueShift), 0.8, 1.0));
    }
    // пламя: излучение чёрного тела — тёмно-красный → оранжевый → жёлто-белый
    return ramp(vec3(0.85, 0.08, 0.02), vec3(1.00, 0.45, 0.06), vec3(1.00, 0.78, 0.35), t);
}

void main() {
    float speed = length(iVel);
    float t = clamp(speed / 300.0, 0.0, 1.0);
    float h = float(pcg(uint(gl_InstanceID)) & 1023u) / 1023.0;

    vec2 dir = speed > 0.5 ? iVel / speed : vec2(1.0, 0.0);
    vec2 perp = vec2(-dir.y, dir.x);
    float size = uSize * (0.65 + 0.7 * h);
    float stretch = 1.0 + speed * uStretch;
    vec2 offset = dir * (aCorner.x * size * stretch) + perp * (aCorner.y * size);

    vUV = aCorner;
    // Яркость растёт с «температурой»; энергия штриха сохраняется при растяжении
    // (длинный штрих тусклее на единицу площади), поэтому взрыв не «выжигает» кадр.
    float energy = 0.35 + 0.75 * t * t;
    vColor = palette(t, h) * (0.8 + 0.4 * h) * energy * uIntensity / stretch;
    gl_Position = uProjection * vec4(iPos + offset, 0.0, 1.0);
}
