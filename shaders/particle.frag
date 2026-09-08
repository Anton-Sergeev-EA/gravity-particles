#version 330 core
in vec2 vUV;
in vec3 vColor;

uniform float uIntensity;

out vec4 FragColor;

void main() {
    float dist = length(vUV);
    if (dist > 1.0) discard;

    // Мягкое радиальное затухание: яркий центр, плавно гаснущий край.
    float glow = pow(1.0 - dist, 2.2);
    vec3 color = vColor * glow * uIntensity;

    FragColor = vec4(color, glow);
}
