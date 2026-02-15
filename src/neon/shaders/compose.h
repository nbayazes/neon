#pragma once

#include "Graphics/ShaderTypes.h"

namespace neon::gfx::shaders::compose {
    enum RootParameterIndex : uint {
        Texture,
        Sampler
    };

    constexpr void SetSource(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        commandList->SetGraphicsRootDescriptorTable(Texture, handle);
    }

    constexpr void SetSampler(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE sampler) {
        commandList->SetGraphicsRootDescriptorTable(Sampler, sampler);
    }

    constexpr ShaderInfo info = {
        .file = "shaders/compose.hlsl",
    };
}

namespace neon::gfx::pipelines {
    inline PipelineInfo compose = {
        .name = "compose pipeline",
        .shader = &shaders::compose::info,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM, // swapchain format
        .blend = BlendMode::Alpha,
        .culling = CullMode::None,
        .depth = DepthMode::None,
        .stencil = StencilMode::None,
        .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .enableMultisample = false,
    };
}
