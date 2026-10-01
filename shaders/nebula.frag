#version 330 core
// Процедурная туманность: фрактальный шум с доменным искажением
// (domain warping), медленно дрейфующий во времени. Рендерится в пониженном
// разрешении — изображение мягкое, а экономия заметная.

uniform vec2 uResolution;
uniform float uTime;
uniform vec3 uColorA;
uniform vec3 uColorB;
uniform vec3 uColorC;
uniform int uOctaves;
uniform float uIntensity;

out vec4 FragColor;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float sum = 0.0;
    float amp = 0.5;
    const mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
    for (int i = 0; i < 8; ++i) {
        if (i >= uOctaves) break;
        sum += amp * noise(p);
        p = rot * p * 2.03 + vec2(1.7, 9.2);
        amp *= 0.5;
    }
    return sum;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y * 2.4;
    float t = uTime * 0.012;

    vec2 q = vec2(fbm(uv + vec2(0.0, t)), fbm(uv + vec2(5.2, 1.3) - t));
    vec2 r = vec2(fbm(uv + 3.2 * q + vec2(1.7, 9.2) + 0.6 * t),
                  fbm(uv + 3.2 * q + vec2(8.3, 2.8) - 0.4 * t));
    float f = fbm(uv + 2.6 * r);

    vec3 col = mix(uColorA, uColorB, clamp(f * f * 2.2, 0.0, 1.0));
    col = mix(col, uColorC, clamp(length(q) * 0.9 - 0.25, 0.0, 1.0));
    // Плотность газа и тёмные пылевые прожилки.
    float density = pow(clamp(f * 1.35, 0.0, 1.0), 2.6);
    float dust = smoothstep(0.25, 0.75, fbm(uv * 2.7 + r * 1.5));
    col *= density * (0.35 + 0.65 * dust);

    // Мягкое угасание к краям кадра — глубина сцены.
    vec2 n = gl_FragCoord.xy / uResolution - 0.5;
    col *= 1.0 - 0.6 * dot(n, n);
    FragColor = vec4(col * uIntensity, 1.0);
}
