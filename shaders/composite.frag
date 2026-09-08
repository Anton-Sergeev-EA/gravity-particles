#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomStrength;

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;

    // Аддитивная композиция: резкая сцена + размытое свечение поверх (bloom).
    vec3 result = scene + bloom * uBloomStrength;

    // Простой мягкий tone-mapping (Reinhard), чтобы яркие зоны не "выжигались" в белое.
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0 / 1.8)); // лёгкая гамма-коррекция для сочности цвета

    FragColor = vec4(result, 1.0);
}
