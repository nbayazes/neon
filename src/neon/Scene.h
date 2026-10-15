#pragma once
#include "Graphics/ParticleSystem.h"
#include "Handles.h"
#include "neon-math.h"
#include "VisualEffect.h"

namespace neon {

struct ModelInstance {
    ModelID model = ModelID::None;
    Matrix transform;
};

struct Scene {
    std::vector<ModelInstance> models;
    std::vector<gfx::ParticleSystem> particles;
    std::vector<gfx::ShockwaveEffect> shockwaves;
};

}
