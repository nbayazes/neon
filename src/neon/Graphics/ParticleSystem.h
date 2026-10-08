#pragma once
#include "GraphicsHandles.h"
#include "neon-math.h"
#include "neon-types.h"
#include "Utility.h"
#include "Random.h"

namespace neon::gfx {

// Uniformly distributed direction on the unit sphere
inline Vector3 RandomDirection() {
    auto t = RandomFloat() * DirectX::XM_2PI;
    float u = RandomFloat(-1, 1);
    auto sqr = sqrt(1 - u * u);
    return { cos(t) * sqr, sin(t) * sqr, u };
}

inline Vector3 RandomPointOnHemisphere() {
    auto a = RandomFloat() * DirectX::XM_2PI;
    auto z = std::asin(std::sqrt(RandomFloat()));
    return { sin(z) * cos(a), sin(z) * sin(a), cos(z) };
}

// Uniformly distributed direction on the unit sphere
//inline Vector3 RandomDirection() {
//    float z = RandomN11();
//    float angle = Random() * DirectX::XM_2PI;
//    float r = sqrt(1.0f - z * z);
//    return { r * cos(angle), r * sin(angle), z };
//}

// Returns a random point on the edge of a circle
inline Vector3 RandomPointOnCircle(float radius = 1) {
    auto t = RandomFloat() * DirectX::XM_2PI;
    return { cos(t) * radius, sin(t) * radius, 0 };
}

// Random direction inside a cone. A coneRadius of 1 spreads particles 45 degrees off of direction.
inline Vector3 RandomConeDirection(const Matrix3x3& rotation, float coneRadius) {
    // sample a disc at a distance of one, tan(45 degrees) is one
    float spread = tan(std::min(coneRadius, 1.0f) * 45.0f * DegToRad);
    float r = sqrt(RandomFloat()) * spread;
    float angle = RandomFloat() * DirectX::XM_2PI;

    auto result = rotation.Right() * (r * cos(angle)) + rotation.Up() * (r * sin(angle)) + rotation.Forward();
    result.Normalize();
    return result;
}

// Uniformly distributed point inside a sphere
inline Vector3 RandomPointOnSphere(float radius) {
    // the cube root spreads points evenly by volume instead of clumping them in the center
    return RandomDirection() * (radius * pow(RandomFloat(), 1.0f / 3.0f));
}


template <class T>
struct NumericRange {
    T min{}, max{};

    NumericRange() = default;

    NumericRange(T value) : min(value), max(value) {}

    NumericRange(T minimum, T maximum) : min(minimum), max(maximum) {
        if (min > max) std::swap(min, max);
    }

    T GetRandom() {
        if (min == max) return min;
        return T((max - min) * RandomFloat() + min);
    }
};

struct Particle {
    //Vector3 position; // The current position, interpolated from start and endPosition
    Vector3 startPosition, endPosition;
    Vector3 velocity;
    float elapsed = 0;
    float duration = 0;
    Color color;
    float size = 0;
    float rotation = 0;

    static bool IsDead(const Particle& particle) {
        return particle.elapsed >= particle.duration;
    }
};

struct ParticleSystemInfo {
    Vector3 position; // can be in world or object space depending on attach mode
    //Vector3 direction = Vector3::Up; // uses a random direction if zero
    Matrix3x3 orientation = Matrix3x3::Zero;
    float coneRadius; // Used with direction to spread sparks. Value of 1 is 45 degrees.
    Color startColor = Color(1, 1, 1, 1);
    Color midColor = Color(1, 1, 1, 1);
    Color endColor = Color(1, 1, 1, 1);
    float midOffset = 0.5f; // Affects where the midpoint is

    float startSize = 5;
    float midSize = 5;
    float endSize = 0;

    Vector3 gravity; // Constant force applied each update
    Vector3 wind; // Constant force applied each update
    float drag = 0.02f;
    float restitution = 0.8f; // How much velocity to keep after hitting a wall
    float spawnRadius = 0; // Sphere to create new particles in
    TexID texture = TexID::None;

    NumericRange<int> count = 1; // number of particles to create per interval
    NumericRange<float> velocity; // particle velocity range
    NumericRange<float> duration = 2.0f; // individual particle lifespan
    NumericRange<float> interval = 0.08f; // interval between creating new particles

    bool useCollision = false; // collides with world geometry and objects
    bool stretchAnimation = false; // stretches animated textures to match the particle lifespan
    bool randomRotation = true; // Randomly set particle initial rotation
    NumericRange<float> rotationSpeed = 0; // Rotation per second

    void SetForwardDirection(const Vector3& forward) {
        Vector3 fwd = forward;
        if (IsZero(fwd)) {
            orientation = Matrix3x3::Zero;
        }
        else {
            fwd.Normalize();
            orientation = Matrix3x3(VectorToRotation(fwd));
        }
    }
};

constexpr auto PARTICLE_TICK_RATE = 1 / 60.0f;

class ParticleSystem {
    List<Particle> _particles;
    //List<Particle> _drawList; // interpolated state built by the last Draw
    float _nextInterval = 0; // time remaining until the next batch of particles is spawned

    // Creates a batch of particles at the spawn point
    void SpawnParticles() {
        int count = std::max(1, info.count.GetRandom());

        for (int i = 0; i < count; i++) {
            // fill a dead particle before growing the list
            auto iter = ranges::find_if(_particles, Particle::IsDead);

            if (iter == _particles.end()) {
                _particles.push_back();
                iter = _particles.end() - 1;
            }

            auto& particle = *iter;
            Vector3 position = info.position;

            if (info.spawnRadius > 0.0f)
                position += RandomPointOnSphere(info.spawnRadius);

            auto direction = IsZero(info.orientation.Forward())
                ? RandomDirection()
                : RandomConeDirection(info.orientation, info.coneRadius);

            particle.velocity = direction * info.velocity.GetRandom();

            particle.elapsed = 0;
            particle.duration = std::max(0.001f, info.duration.GetRandom());

            particle.color = info.startColor;
            particle.size = info.startSize;

            particle.startPosition = particle.endPosition = position;

            if (info.randomRotation)
                particle.rotation = RandomFloat(0, DirectX::XM_2PI);
            else
                particle.rotation = 0;
        }
    }

public:
    span<const Particle> Particles() const { return _particles; }

    ParticleSystemInfo info = {};

    // Simulate is called at a fixed tick rate
    void Simulate(float dt) {
        _nextInterval -= dt;

        if (_nextInterval <= 0) {
            // spawn particles once the interval passes
            SpawnParticles();

            if (info.interval.min == info.interval.max && info.interval.min == 0)
                _nextInterval = FLT_MAX;
            else
                _nextInterval = info.interval.GetRandom();
        }

        float drag = 1.0f - std::clamp(info.drag, 0.0f, 1.0f);

        for (auto& particle : _particles) {
            // dead particles wait in the pool until a new one reuses them
            if (Particle::IsDead(particle)) continue;

            // remember the last tick so Draw can interpolate between the two
            Vector3 position = particle.endPosition;
            particle.startPosition = position;

            if (!IsZero(info.gravity)) particle.velocity += info.gravity * dt;
            if (!IsZero(info.wind)) particle.velocity += info.wind * dt;
            if (drag < 1) particle.velocity *= drag;
            particle.endPosition = position + particle.velocity * dt;
            particle.elapsed += dt;
        }
    }
};

}
