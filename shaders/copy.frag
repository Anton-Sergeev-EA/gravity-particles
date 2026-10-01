#version 330 core
// Копирование текстуры с множителем: перенос слоёв в сцену и затухание шлейфов.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uScale;

void main() {
    FragColor = vec4(texture(uTexture, vUV).rgb * uScale, 1.0);
}
