#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aSize;        // диаметр спрайта, px
layout(location = 2) in float aBrightness;
layout(location = 3) in float aSpeed;       // скорость мерцания
layout(location = 4) in float aPhase;
layout(location = 5) in vec3 aColor;        // цветовая температура звезды

uniform mat4 uProjection;
uniform float uTime;
uniform float uScale;

out vec3 vColor;
out float vSpikes;
out float vSharp;

void main() {
    float twinkle = 0.65 + 0.35 * sin(uTime * aSpeed + aPhase) * sin(uTime * aSpeed * 0.37 + aPhase * 2.1);
    float big = smoothstep(4.0, 12.0, aSize);
    vColor = aColor * aBrightness * twinkle;
    vSpikes = big;                         // лучи — только у ярких звёзд
    vSharp = mix(9.0, 60.0, big);          // у крупных спрайтов ядро компактное
    gl_PointSize = aSize * uScale;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
