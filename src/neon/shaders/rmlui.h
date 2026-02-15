#pragma once

#include "Graphics/ShaderTypes.h"
#include "neon-math.h"

namespace neon::gfx::shaders::rmlui {
    enum RootParameters : uint {
        Constants,
        Diffuse,
        Sampler,
    };

    struct Vertex {
        Vector2 position;
        uint32 color;
        Vector2 uv;

        static constexpr D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
    };

    constexpr void SetDiffuse(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE texture) {
        commandList->SetGraphicsRootDescriptorTable(Diffuse, texture);
    }

    constexpr void SetProjectionMatrix(ID3D12GraphicsCommandList* commandList, const Matrix& mvp) {
        commandList->SetGraphicsRoot32BitConstants(Constants, sizeof(mvp) / 4, &mvp.m, 0);
    }

    constexpr void SetSampler(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE sampler) {
        commandList->SetGraphicsRootDescriptorTable(Sampler, sampler);
    }

    constexpr ShaderInfo info = {
        .file = "shaders/imgui.hlsl",
        .inputLayout = gfx::CreateLayout(Vertex::layout),
    };
}

namespace neon::gfx::pipelines {
    inline PipelineInfo rmlui = {
        .name = "imgui pipeline",
        .shader = &shaders::rmlui::info,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        .blend = BlendMode::StraightAlpha,
        .culling = CullMode::None,
        .depth = DepthMode::None,
        .stencil = StencilMode::None,
        .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .enableMultisample = false,
    };
}
