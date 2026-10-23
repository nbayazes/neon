#include "pch.h"
#include "Particles.h"
#include <algorithm>
#include "Device.h"
#include "Graphics.h"
#include "SpriteBatch.h"

namespace neon::gfx {

namespace {
    constexpr float TICK_RATE = 1.0f / 64; // 64 ticks per second

    // Scalar quadratic passing through (0,y0), (tm,ym), (1,y1) at t
    float QuadraticInterpolation2D(float t, float y0, float tm, float ym, float y1) {
        if (t <= 0.0f) return y0;
        if (t >= 1.0f) return y1;

        // control point
        float denom_c = 2.0f * tm * (1.0f - tm);
        float c = 0.0f;
        if (std::fabs(denom_c) < 1e-12) {
            // linear fallback
            float w = (t - 0.0f) / (1.0f - 0.0f);
            return y0 + w * (y1 - y0);
        }
        else {
            c = (ym - (1.0f - tm) * (1.0f - tm) * y0 - tm * tm * y1) / denom_c;
        }
        // evaluate quadratic Bezier at t
        float mt = 1.0f - t;
        return mt * mt * y0 + 2.0f * t * mt * c + t * t * y1;
    }
}

void DrawParticleSystem(GraphicsContext& context, const ParticleSystem& system, float tickAlpha) {
    auto& spriteBatch = GetSpriteBatcher().GetSpriteBatch(system.info.blendMode, GetFrameIndex());

    auto offset = tickAlpha * TICK_RATE; // elapsed offset
    auto& info = system.info;

    for (auto& particle : system.Particles()) {
        if (Particle::IsDead(particle)) continue;

        SpriteBatchInfo sprite;
        sprite.vertex.position = Vector3::Lerp(particle.startPosition, particle.endPosition, tickAlpha);

        // interpolate values across the lifespan
        float t = std::clamp((particle.elapsed + offset) / particle.duration, 0.0f, 1.0f);

        auto size = QuadraticInterpolation2D(t, info.startSize, info.midOffset, info.midSize, info.endSize);
        sprite.vertex.size = Vector2{ size, size };;

        if (t < info.midOffset) {
            float segmentLife = t / info.midOffset;
            sprite.vertex.color = Color::Lerp(info.startColor, info.midColor, segmentLife);
            //auto size = info.startSize + (info.midSize - info.startSize) * segmentLife;
            //sprite.vertex.size = Vector2{ size, size };
        }
        else {
            float segmentLife = (t - info.midOffset) / (1 - info.midOffset);
            sprite.vertex.color = Color::Lerp(info.midColor, info.endColor, segmentLife);
            //auto size = info.midSize + (info.endSize - info.midSize) * segmentLife;
            //sprite.vertex.size = Vector2{ size, size };
        }

        // enable animation stretching
        if (info.stretchAnimation)
            sprite.vertex.percentLife = t;

        sprite.vertex.rotation = particle.rotation;
        sprite.texture = info.texture;

        if (RequiresSorting(info.blendMode))
            sprite.depth = Vector3::DistanceSquared(context.camera->Position, sprite.vertex.position);

        spriteBatch.Add(sprite);
    }
}

}
