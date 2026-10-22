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
    std::vector<gfx::VisualEffect> effects;

    void AddParticleSystem(const gfx::ParticleSystem& effect) {
        for (auto& item : particles) {
            if (item.elapsed > item.info.lifespan && item.info.lifespan > 0) {
                item = effect;
                return;
            }
        }

        particles.push_back(effect);
    }

    void AddShockwave(gfx::ShockwaveEffect effect) {
        if (effect.randomRotation) {
            effect.rotation = VectorToRotation(RandomDirection());
            effect.cameraRotation = RandomFloat(0, DirectX::XM_2PI);
        }

        for (auto& item : shockwaves) {
            if (item.elapsed > item.duration && item.duration > 0) {
                item = effect;
                return;
            }
        }

        shockwaves.push_back(effect);
    }
};

}
