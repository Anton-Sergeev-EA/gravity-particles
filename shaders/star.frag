#version 330 core
in vec3 vColor;
in float vSpikes;
in float vSharp;

out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * vSharp);
    // Дифракционные лучи (как от растяжек вторичного зеркала телескопа).
    float spikes = exp(-abs(c.x) * 38.0) * exp(-abs(c.y) * 2.6) +
                   exp(-abs(c.y) * 38.0) * exp(-abs(c.x) * 2.6);
    float halo = exp(-r2 * 6.0) * 0.12;
    float fade = 1.0 - r2;
    FragColor = vec4(vColor * (core + (spikes * 0.9 + halo) * vSpikes) * fade, 1.0);
}
