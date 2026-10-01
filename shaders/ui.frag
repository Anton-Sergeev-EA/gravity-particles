#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vShape;
in vec3 vParams;

uniform sampler2D uAtlas;

out vec4 FragColor;

// Знаковое расстояние до скруглённого прямоугольника (Inigo Quilez).
float sdRoundBox(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    float mode = vParams.y;
    if (mode < 0.5) {
        // Глиф: покрытие из атласа с лёгкой гамма-поправкой для светлого текста на тёмном.
        float coverage = texture(uAtlas, vUV).r;
        coverage = pow(coverage, 0.85);
        FragColor = vec4(vColor.rgb, vColor.a * coverage);
        return;
    }

    float d = sdRoundBox(vShape.xy, vShape.zw, vParams.x);
    float alpha = clamp(0.5 - d, 0.0, 1.0);
    if (mode > 1.5) {
        // Обводка: полоса шириной vParams.z внутри границы.
        alpha *= clamp(0.5 + d + vParams.z, 0.0, 1.0);
    }
    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
