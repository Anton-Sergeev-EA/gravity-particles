#version 330 core
in vec2 vUV;
in vec3 vColor;

out vec4 FragColor;

void main() {
    float d2 = dot(vUV, vUV);
    if (d2 > 1.0) discard;
    // Гауссово ядро: горячий центр, мягко гаснущий край.
    float glow = exp(-d2 * 4.0) - 0.0183;
    FragColor = vec4(vColor * glow, 1.0);
}
