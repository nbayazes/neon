#pragma once
#include "neon-math.h"
#include "GraphicsHandles.h"

namespace neon::gfx {

class GraphicsContext;
struct Mesh;

struct ShockwaveEffect {
    Vector3 position;
    Matrix3x3 rotation;
    float duration = 0.5f;
    float elapsed = 0;
    float radius = 5.0f;
    uint count = 1;
    TexID texture = TexID::None;
    bool randomRotation = false;
    bool faceCamera = false;
    float cameraRotation = 0; // When facing the camera, rotates by this amount. Random rotation overrides this.
    Color color = Color(1, 1, 1);
};

Mesh GenerateShockwaveMesh(uint16 segments, uint16 textureRepeat, float thickness);

void DrawShockwave(GraphicsContext& context, const ShockwaveEffect& shockwave);

}
