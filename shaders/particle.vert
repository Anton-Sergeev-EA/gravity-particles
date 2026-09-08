#version 330 core
layout(location = 0) in vec2 aCorner;   // угол базового квада (-1..1)
layout(location = 1) in vec2 iPos;      // позиция частицы (world space)
layout(location = 2) in vec3 iColor;    // цвет частицы
layout(location = 3) in float iSize;    // радиус частицы

uniform mat4 uProjection;
uniform float uSizeMultiplier;

out vec2 vUV;
out vec3 vColor;

void main() {
    vUV = aCorner;
    vColor = iColor;
    vec2 worldPos = iPos + aCorner * iSize * uSizeMultiplier;
    gl_Position = uProjection * vec4(worldPos, 0.0, 1.0);
}
