#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aShape;   // xy — точка относительно центра, zw — полуразмеры
layout(location = 4) in vec3 aParams;  // x — радиус скругления, y — режим, z — толщина обводки

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;
out vec4 vShape;
out vec3 vParams;

void main() {
    vUV = aUV;
    vColor = aColor;
    vShape = aShape;
    vParams = aParams;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
