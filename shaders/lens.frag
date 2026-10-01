#version 330 core
// Чёрные дыры и ударные волны в экранном пространстве:
// * гравитационное линзирование — изображение за дырой отклоняется к ней,
//   образуя кольцо Эйнштейна;
// * горизонт событий — абсолютно чёрный диск;
// * фотонное кольцо и свечение аккреции (HDR — затем подхватывается bloom);
// * ударные волны — кольцевое искажение с ярким фронтом.

in vec2 vUV;
out vec4 FragColor;

const int MAX_HOLES = 32;
const int MAX_WAVES = 8;

uniform sampler2D uScene;
uniform vec2 uResolution;
uniform int uHoleCount;
uniform vec4 uHoles[MAX_HOLES];  // xy — центр (px, ось Y вверх), z — радиус горизонта, w — масса/600
uniform int uWaveCount;
uniform vec4 uWaves[MAX_WAVES];  // xy — центр, z — радиус фронта, w — амплитуда
uniform float uLensing;          // 0 — выключено, 1 — включено
uniform vec3 uRingColor;
uniform vec3 uGlowColor;
uniform float uTime;

void main() {
    vec2 p = gl_FragCoord.xy;
    vec2 offset = vec2(0.0);
    float mask = 1.0;
    vec3 emission = vec3(0.0);

    for (int i = 0; i < MAX_HOLES; ++i) {
        if (i >= uHoleCount) break;
        vec2 d = p - uHoles[i].xy;
        float r = length(d) + 1e-4;
        float rs = uHoles[i].z;
        vec2 dir = d / r;

        // Отклонение лучей ~ rs² / r (слабое поле), плавно ограниченное у горизонта.
        float bend = uLensing * 2.6 * rs * rs / max(r, rs * 0.7);
        offset -= dir * bend;

        mask *= smoothstep(rs * 0.92, rs * 1.04, r);

        // Фотонное кольцо на ~1.5 rs и асимметричное (доплеровское) свечение аккреции.
        float ringW = rs * 0.16 + 0.8;
        float ring = exp(-pow((r - rs * 1.5) / ringW, 2.0));
        float doppler = 0.75 + 0.45 * dir.x;
        float glow = exp(-(r - rs) / (rs * 1.8)) * step(rs, r);
        float flicker = 0.92 + 0.08 * sin(uTime * 3.0 + float(i) * 1.7);
        emission += (uRingColor * ring * 3.2 * doppler + uGlowColor * glow * 0.55) * flicker;
    }

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount) break;
        vec2 d = p - uWaves[i].xy;
        float r = length(d) + 1e-4;
        float width = 26.0 + uWaves[i].z * 0.06;
        float x = (r - uWaves[i].z) / width;
        float profile = exp(-x * x);
        offset += (d / r) * uWaves[i].w * profile * x * 18.0;
        emission += uGlowColor * profile * uWaves[i].w * 0.6;
    }

    vec2 uv = (p + offset) / uResolution;
    vec3 scene = texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
    FragColor = vec4((scene + emission) * mask, 1.0);
}
