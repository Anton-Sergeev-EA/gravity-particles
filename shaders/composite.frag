#version 330 core
// Финальный кадр: сцена + bloom → экспозиция → ACES-тонмаппинг → sRGB,
// плюс кинематографические эффекты: хроматическая аберрация, виньетка,
// плёночное зерно (оно же дизеринг против полос на градиентах) и тряска камеры.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uAdapted;        // 1×1 — адаптированная средняя яркость
uniform float uKey;                // целевая средняя яркость кадра
uniform float uBloomStrength;
uniform float uExposure;
uniform float uTime;
uniform int uCinematic;
uniform vec2 uResolution;
uniform vec2 uShake;

// Аппроксимация ACES RRT+ODT (Stephen Hill) — тот же тонмаппер, что в UE4.
const mat3 ACES_IN = mat3(0.59719, 0.07600, 0.02840,
                          0.35458, 0.90834, 0.13383,
                          0.04823, 0.01566, 0.83777);
const mat3 ACES_OUT = mat3(1.60475, -0.10208, -0.00327,
                           -0.53108, 1.10813, -0.07276,
                           -0.07367, -0.00605, 1.07602);

vec3 rrtOdtFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 aces(vec3 c) {
    return clamp(ACES_OUT * rrtOdtFit(ACES_IN * c), 0.0, 1.0);
}

vec3 toSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

vec3 hdr(vec2 uv) {
    return texture(uScene, uv).rgb + texture(uBloom, uv).rgb * uBloomStrength;
}

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = vUV + uShake;
    vec3 color;
    vec2 fromCenter = uv - 0.5;
    if (uCinematic == 1) {
        // Хроматическая аберрация нарастает к краям кадра, как у реальной оптики.
        vec2 ca = fromCenter * dot(fromCenter, fromCenter) * 0.007;
        color = vec3(hdr(uv - ca).r, hdr(uv).g, hdr(uv + ca).b);
    } else {
        color = hdr(uv);
    }

    // Экспозиция подстраивается под среднюю яркость (только вниз — ночная сцена
    // остаётся тёмной, а вспышка взрыва не выжигает кадр).
    float adapted = texture(uAdapted, vec2(0.5)).r;
    float autoExposure = clamp(pow(uKey / max(adapted, 1e-4), 0.9), 0.08, 1.0);
    color = aces(color * uExposure * autoExposure);
    // Цветокоррекция: ACES приглушает насыщенность ярких тонов — немного возвращаем её.
    float grey = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = clamp(mix(vec3(grey), color, 1.15), 0.0, 1.0);

    if (uCinematic == 1) {
        float aspect = uResolution.x / uResolution.y;
        vec2 v = fromCenter * vec2(aspect, 1.0);
        color *= mix(1.0, smoothstep(1.15, 0.25, length(v)), 0.55);
    }

    color = toSrgb(color);
    // Зерно / дизеринг: убирает «ступеньки» на тёмных градиентах туманности.
    float grain = hash(gl_FragCoord.xy + fract(uTime * 13.7) * 977.0) - 0.5;
    color += grain * (uCinematic == 1 ? 0.028 : 1.0 / 255.0);
    FragColor = vec4(color, 1.0);
}
