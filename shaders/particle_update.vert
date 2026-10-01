#version 330 core
// GPU-физика частиц: каждый вызов вершинного шейдера обновляет одну частицу,
// результат записывается в буфер через transform feedback (без растеризации).

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aVel;

out vec2 oPos;
out vec2 oVel;

const int MAX_ATTRACTORS = 32;
const float SOFTENING2 = 400.0;  // смягчение гравитации (20 px), как в исходной версии

uniform float uDt;
uniform float uGravity;
uniform float uDamping;
uniform vec2 uWorld;
uniform int uCount;
uniform vec4 uAttr[MAX_ATTRACTORS];  // xy — позиция, z — масса, w — радиус горизонта
uniform float uExplode;               // > 0 — в этом кадре взрыв
uniform int uReset;                   // 1 — разбросать все частицы заново
uniform uint uSeed;
uniform int uBurstIndex;              // >= 0 — новая дыра «собирает» себе диск
uniform float uBurstFraction;         // доля частиц, переносимых к ней

uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rnd(inout uint s) {
    s = pcg(s);
    return float(s) * (1.0 / 4294967295.0);
}

// Рождение частицы на круговой орбите вокруг случайного источника:
// так формируются вращающиеся аккреционные диски.
void spawn(inout vec2 p, inout vec2 v, inout uint s, int forced) {
    if (uCount == 0) {
        p = vec2(rnd(s), rnd(s)) * uWorld;
        v = vec2(0.0);
        return;
    }
    int i = forced >= 0 ? forced : min(uCount - 1, int(rnd(s) * float(uCount)));
    vec4 a = uAttr[i];
    float minDim = min(uWorld.x, uWorld.y);
    float rmin = a.w * 2.2 + 4.0;
    float rmax = max(rmin + 20.0, minDim * 0.44 * clamp(sqrt(a.z / 600.0), 0.6, 1.4));
    // Экспоненциальный профиль диска, как у реальных спиральных галактик:
    // плотное яркое ядро и разреженная протяжённая периферия.
    float r = rmin + rmax * 0.32 * -log(max(1.0 - rnd(s) * 0.985, 1e-4));
    float ang;
    if (rnd(s) < 0.62) {
        // Две логарифмические спиральные ветви с разбросом.
        float arm = floor(rnd(s) * 2.0) * 3.14159265;
        float spread = (rnd(s) + rnd(s) + rnd(s) - 1.5) * 0.45;
        ang = arm + 2.4 * log(r / rmin) + spread + float(i) * 1.3;
    } else {
        ang = rnd(s) * 6.2831853;
    }
    vec2 dir = vec2(cos(ang), sin(ang));
    p = a.xy + dir * r;

    float gm = 900.0 * uGravity * a.z;
    float acc = gm * r / pow(r * r + SOFTENING2, 1.5);
    float vOrbit = sqrt(acc * r);
    vec2 tangent = vec2(-dir.y, dir.x);
    v = tangent * vOrbit * mix(0.82, 1.04, rnd(s)) + dir * (rnd(s) - 0.5) * vOrbit * 0.12;
}

void main() {
    uint s = uint(gl_VertexID) * 9781u + uSeed * 6271u + 1u;
    vec2 p = aPos;
    vec2 v = aVel;

    if (uReset == 1) {
        spawn(p, v, s, -1);
        oPos = p;
        oVel = v;
        return;
    }

    vec2 force = vec2(0.0);
    float nearest = 1e9;
    vec2 nearestPos = p;
    bool swallowed = false;
    for (int i = 0; i < uCount; ++i) {
        vec4 a = uAttr[i];
        vec2 d = a.xy - p;
        float d2 = dot(d, d) + SOFTENING2;
        float dist = sqrt(d2);
        force += d / dist * (900.0 * uGravity * a.z / d2);
        float r = length(d);
        if (r < nearest) {
            nearest = r;
            nearestPos = a.xy;
        }
        if (r < a.w * 0.85) swallowed = true;  // частица упала за горизонт событий
    }

    if (uExplode > 0.0) {
        // Взрыв: радиальный выброс от ближайшего источника с закруткой.
        vec2 away = p - nearestPos;
        float l = length(away);
        vec2 dir = l > 0.001 ? away / l : vec2(1.0, 0.0);
        float speed = mix(180.0, 520.0, rnd(s)) * uExplode;
        v = dir * speed + vec2(-dir.y, dir.x) * speed * 0.35 * (rnd(s) - 0.3);
        swallowed = false;
    }

    if (uBurstIndex >= 0 && uBurstIndex < uCount && rnd(s) < uBurstFraction) {
        spawn(p, v, s, uBurstIndex);
        oPos = p;
        oVel = v;
        return;
    }

    v = (v + force * uDt) * uDamping;
    float speed = length(v);
    if (speed > 2600.0) v *= 2600.0 / speed;  // защита от «рогатки» у сингулярности
    p += v * uDt;

    const float margin = 140.0;
    if (swallowed || p.x < -margin || p.y < -margin || p.x > uWorld.x + margin ||
        p.y > uWorld.y + margin) {
        spawn(p, v, s, -1);
    }
    oPos = p;
    oVel = v;
}
