#pragma once

// Минимальная математика без внешних зависимостей (без GLM).
// Строит стандартную ортографическую проекционную матрицу (column-major, как ожидает OpenGL).
inline void orthoMatrix(float left, float right, float bottom, float top,
                         float nearZ, float farZ, float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0]  = 2.0f / (right - left);
    out[5]  = 2.0f / (top - bottom);
    out[10] = -2.0f / (farZ - nearZ);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(farZ + nearZ) / (farZ - nearZ);
    out[15] = 1.0f;
}
