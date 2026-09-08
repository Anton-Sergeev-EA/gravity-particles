#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aBrightness;
layout(location = 2) in float aSpeed;
layout(location = 3) in float aPhase;

uniform mat4 uProjection;
uniform float uTime;

out float vBrightness;

void main() {
    float twinkle = 0.5 + 0.5 * sin(uTime * aSpeed + aPhase);
    vBrightness = aBrightness * (0.5 + 0.5 * twinkle);
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    gl_PointSize = 1.8;
}
