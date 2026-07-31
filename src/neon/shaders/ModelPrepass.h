#pragma once

#include "Graphics/ShaderTypes.h"
#include "ModelVertex.h"
#include "neon-math.h"

namespace neon::gfx::shaders::ModelPrepass {
enum RootParameters : uint {
    Constants,
    Diffuse,
};

struct Arguments {
    Matrix projection;
    Vector2 translation;
};

constexpr void SetProjectionMatrix(ID3D12GraphicsCommandList* commandList, const Arguments& args) {
    commandList->SetGraphicsRoot32BitConstants(Constants, sizeof(args) / 4, &args, 0);
}

constexpr ShaderInfo info = {
    .file = "shaders/model_prepass.hlsl",
    .inputLayout = gfx::CreateLayout(ModelVertex::layout),
};
}

namespace neon::gfx::pipelines {
inline PipelineInfo model = {
    .name = "model prepass",
    .shader = &shaders::ModelPrepass::info,
    .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .blend = BlendMode::Opaque,
    .culling = CullMode::None,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};
}
