#pragma once
#include <cmath>

// Column-major math only. Keep Apple's GLKit/OpenGLES out of the ANGLE linker path.
struct ViewerMatrix4 {
    float m[16];
};

static inline ViewerMatrix4 ViewerMatrix4MakeTranslation(float x, float y, float z) {
    return ViewerMatrix4{{
        1, 0, 0, 0, // Column 0
        0, 1, 0, 0, // Column 1
        0, 0, 1, 0, // Column 2
        x, y, z, 1, // Column 3
    }};
}
static inline ViewerMatrix4 ViewerMatrix4MakeYRotation(float angle) {
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return ViewerMatrix4{{
        cosine, 0, -sine, 0, // Column 0
        0, 1, 0, 0,          // Column 1
        sine, 0, cosine, 0,  // Column 2
        0, 0, 0, 1,          // Column 3
    }};
}
static inline ViewerMatrix4 ViewerMatrix4MakeOrtho(float left, float right, float bottom, float top,
                                                   float nearPlane, float farPlane) {
    return ViewerMatrix4{{2 / (right - left), 0, 0, 0, 0, 2 / (top - bottom), 0, 0, 0, 0,
                          -2 / (farPlane - nearPlane), 0, -(right + left) / (right - left),
                          -(top + bottom) / (top - bottom),
                          -(farPlane + nearPlane) / (farPlane - nearPlane), 1}};
}
static inline ViewerMatrix4 ViewerMatrix4Multiply(ViewerMatrix4 a, ViewerMatrix4 b) {
    ViewerMatrix4 result = {{0}};
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            for (int k = 0; k < 4; k++) {
                result.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
        }
    }
    return result;
}
