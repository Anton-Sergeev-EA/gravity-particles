#version 330 core
// Повышающая выборка bloom: «шатровый» фильтр 3×3, результат аддитивно
// накладывается на следующий, более крупный уровень цепочки.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSource;
uniform vec2 uTexel;
uniform float uRadius;
uniform float uWeight;

void main() {
    vec2 t = uTexel * uRadius;
    vec3 sum = texture(uSource, vUV).rgb * 4.0;
    sum += (texture(uSource, vUV + vec2(-t.x, 0.0)).rgb + texture(uSource, vUV + vec2(t.x, 0.0)).rgb +
            texture(uSource, vUV + vec2(0.0, -t.y)).rgb + texture(uSource, vUV + vec2(0.0, t.y)).rgb) * 2.0;
    sum += texture(uSource, vUV + vec2(-t.x, -t.y)).rgb + texture(uSource, vUV + vec2(t.x, -t.y)).rgb +
           texture(uSource, vUV + vec2(-t.x, t.y)).rgb + texture(uSource, vUV + vec2(t.x, t.y)).rgb;
    FragColor = vec4(sum / 16.0 * uWeight, 1.0);
}
