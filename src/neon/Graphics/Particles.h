#pragma once

namespace neon::gfx {

class GraphicsContext;
class ParticleSystem;

void DrawParticleSystem(GraphicsContext& context, const ParticleSystem& system, float tickAlpha);

}
