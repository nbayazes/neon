#pragma once
#include <variant>
#include "Graphics/Mesh.h"
#include "Graphics/ParticleSystem.h"
#include "Graphics/Shockwave.h"

namespace neon::gfx {

struct SoundEffect {
    Vector3 position;
    string sound;
    float volume = 1;
    float pitch = 1;
    bool looping = false;
};

enum class EffectType {
    None, ParticleSystem, Shockwave, Sound
};

// When created a visual effect inserts its contents
// into the world
struct VisualEffect {
    struct Item {
        float timeOffset = 0; // offset in the timeline
        string name; // Name in the effect editor
        EffectType type = EffectType::None;

        std::variant<ParticleSystem, ShockwaveEffect, SoundEffect> data;
    };

    List<Item> items;

    // if looped the effect repeats instead of only playing once
    int repeats = 0; // -1 repeats forever
    float duration = 0; // used for looping
    float elapsed = 0;

    // effect needs to know when to create instances
    void Update(float dt);
};

}
