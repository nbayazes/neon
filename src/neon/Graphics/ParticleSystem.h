#pragma once
#include "neon-math.h"
#include "neon-types.h"

namespace neon::gfx {

struct Particle {
    Vector3 position, prevPosition;
    Vector3 velocity;
    float elapsed = 0;
    Color color, prevColor;
    float size = 0;
    float prevSize = 0;
};

struct ParticleSystemInfo {
    Vector3 position;
    Color startColor;
    Color endColor;
    float startSize;
    float endSize;
    Vector3 gravity;
    float drag;
    int count;
};

class ParticleSystem {
    List<Particle> _particles;
    uint64 _last = 0;
public:

    void Update(float dt) {
        
    }
};

}
