#pragma once

#include "Common.h"
#include "Graphics/GraphicsHandles.h"
#include "Graphics/ShaderTypes.h"
#include "neon-math.h"

namespace neon::gfx::shaders::shockwave {
struct Vertex {
    Vector3 position;
    Vector2 uv;
    Color color;

    static constexpr D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
};

struct Constants {
    Matrix transform;
    TexID texture = TexID::None;
};

constexpr D3D12_DESCRIPTOR_RANGE1 Descriptors[] = {
    {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0 // b0. Instance constants
    }
};

constexpr D3D12_ROOT_PARAMETER1 Params[] = {
    FrameConstantsParameter,
    TextureTableParameter,
    TextureInfoTableParameter,
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {.NumDescriptorRanges = std::size(Descriptors), .pDescriptorRanges = Descriptors },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
};

constexpr D3D12_ROOT_SIGNATURE_DESC1 RootSignature = {
    .NumParameters = std::size(Params),
    .pParameters = Params,
    .NumStaticSamplers = 1,
    .pStaticSamplers = &LinearSampler,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
};

constexpr ShaderInfo info = {
    .file = "shaders/Shockwave.hlsl",
    .inputLayout = gfx::CreateLayout(Vertex::layout),
    .rootSignature = &RootSignature
};

}

namespace neon::gfx::pipelines {

inline PipelineInfo shockwave = {
    .name = "shockwave",
    .shader = &shaders::shockwave::info,
    .format = DXGI_FORMAT_R11G11B10_FLOAT,
    .blend = BlendMode::Additive,
    .culling = CullMode::CounterClockwise,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};

}
