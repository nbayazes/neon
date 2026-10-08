#pragma once
#include "Graphics/ParticleSystem.h"
#include "Handles.h"
#include "neon-math.h"

namespace neon {

struct ModelInstance {
    ModelID model = ModelID::None;
    Vector3 position;
    Matrix3x3 rotation;
};

struct Scene {
    std::vector<ModelInstance> models;
    std::vector<gfx::ParticleSystem> particles;
};

}
