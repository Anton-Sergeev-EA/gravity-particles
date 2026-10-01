#version 330 core
// Адаптация глаза (auto-exposure): средняя логарифмическая яркость кадра
// по самому мелкому уровню bloom, сглаженная во времени. При яркой вспышке
// экспозиция плавно снижается, затем возвращается — как в игровых движках.
out vec4 FragColor;

uniform sampler2D uLuminance;  // самый мелкий уровень цепочки bloom
uniform sampler2D uPrevious;   // адаптированная яркость прошлого кадра (1×1)
uniform float uDt;             // шаг времени кадра, с

void main() {
    float sum = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 uv = (vec2(x, y) + 0.5) / 4.0;
            float l = dot(texture(uLuminance, uv).rgb, vec3(0.2126, 0.7152, 0.0722));
            sum += log(l + 1e-4);
        }
    }
    float current = exp(sum / 16.0);
    float previous = texture(uPrevious, vec2(0.5)).r;
    // К свету глаз привыкает быстро (~0,1 с), к темноте — медленно (~1,2 с).
    float speed = current > previous ? 10.0 : 0.8;
    float adapted = previous <= 0.0 ? current : mix(previous, current, 1.0 - exp(-uDt * speed));
    FragColor = vec4(adapted, adapted, adapted, 1.0);
}
