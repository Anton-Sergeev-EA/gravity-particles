#version 330 core
// Понижающая выборка bloom: 13-точечный фильтр (Jimenez, «Next Generation
// Post Processing in Call of Duty: Advanced Warfare»). На первом уровне —
// взвешивание Кариса, подавляющее мерцание одиночных ярких пикселей.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSource;
uniform vec2 uTexel;   // размер текселя источника
uniform int uKaris;

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
vec3 karis(vec3 c) { return c / (1.0 + luma(c)); }

void main() {
    vec2 t = uTexel;
    vec3 a = texture(uSource, vUV + t * vec2(-2.0, 2.0)).rgb;
    vec3 b = texture(uSource, vUV + t * vec2(0.0, 2.0)).rgb;
    vec3 c = texture(uSource, vUV + t * vec2(2.0, 2.0)).rgb;
    vec3 d = texture(uSource, vUV + t * vec2(-2.0, 0.0)).rgb;
    vec3 e = texture(uSource, vUV).rgb;
    vec3 f = texture(uSource, vUV + t * vec2(2.0, 0.0)).rgb;
    vec3 g = texture(uSource, vUV + t * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(uSource, vUV + t * vec2(0.0, -2.0)).rgb;
    vec3 i = texture(uSource, vUV + t * vec2(2.0, -2.0)).rgb;
    vec3 j = texture(uSource, vUV + t * vec2(-1.0, 1.0)).rgb;
    vec3 k = texture(uSource, vUV + t * vec2(1.0, 1.0)).rgb;
    vec3 l = texture(uSource, vUV + t * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(uSource, vUV + t * vec2(1.0, -1.0)).rgb;

    vec3 result;
    if (uKaris == 1) {
        vec3 g0 = (a + b + d + e) * 0.25;
        vec3 g1 = (b + c + e + f) * 0.25;
        vec3 g2 = (d + e + g + h) * 0.25;
        vec3 g3 = (e + f + h + i) * 0.25;
        vec3 g4 = (j + k + l + m) * 0.25;
        float w0 = 1.0 / (1.0 + luma(g0));
        float w1 = 1.0 / (1.0 + luma(g1));
        float w2 = 1.0 / (1.0 + luma(g2));
        float w3 = 1.0 / (1.0 + luma(g3));
        float w4 = 1.0 / (1.0 + luma(g4));
        result = (g0 * w0 * 0.125 + g1 * w1 * 0.125 + g2 * w2 * 0.125 + g3 * w3 * 0.125 + g4 * w4 * 0.5) /
                 (w0 * 0.125 + w1 * 0.125 + w2 * 0.125 + w3 * 0.125 + w4 * 0.5);
    } else {
        result = e * 0.125;
        result += (a + c + g + i) * 0.03125;
        result += (b + d + f + h) * 0.0625;
        result += (j + k + l + m) * 0.125;
    }
    FragColor = vec4(max(result, vec3(0.0)), 1.0);
}
