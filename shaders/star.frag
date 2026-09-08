#version 330 core
in float vBrightness;
out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) discard;
    FragColor = vec4(vec3(vBrightness, vBrightness, vBrightness * 1.1), 1.0);
}
