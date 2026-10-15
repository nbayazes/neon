#pragma once
#include "Graphics/Mesh.h"
#include "Graphics/ParticleSystem.h"

namespace neon::gfx {

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

// struct VisualEffectInstance {
//     float timelineOffset = 0;
//     List<ParticleSystemInfo> particles;
// };

// When created a visual effect inserts its contents
// into the world
struct VisualEffect {
    struct Item {
        float timeOffset = 0;
        List<ParticleSystemInfo> particles;
        // all the other effect types
    };

    // if looped the effect repeats instead of only playing once
    int repeats = 0; // -1 repeats forever
    float duration = 0;
    float elapsed = 0;

    // effect needs to know when to create instances
    void Update(float dt);
};

Mesh GenerateShockwaveMesh(uint16 segments, uint16 textureRepeat, float thickness);

//void DrawShockwave(RenderContext& context, Vector3 position, Vector3 normal);
}
