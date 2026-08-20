#pragma once

#include "Common.h"
#include "Graphics/ShaderTypes.h"
#include "ModelVertex.h"
#include "neon-math.h"

namespace neon::gfx::shaders::ModelPrepass {

constexpr D3D12_DESCRIPTOR_RANGE1 Descriptors[] = {
    {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0 // b0. Instance constants
    },
    {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0, // t0. Texture indices
        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
    },
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

constexpr D3D12_STATIC_SAMPLER_DESC LinearSampler{
    .Filter = D3D12_FILTER_ANISOTROPIC,
    .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .MipLODBias = 0,
    .MaxAnisotropy = 0,
    .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
    .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
    .MinLOD = 0,
    .MaxLOD = 0,
    .ShaderRegister = 0,
    .RegisterSpace = 0,
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    // .Flags = D3D12_SAMPLER_FLAG_NONE
};

constexpr D3D12_ROOT_SIGNATURE_DESC1 RootSignature = {
    .NumParameters = std::size(Params),
    .pParameters = Params,
    .NumStaticSamplers = 1,
    .pStaticSamplers = &LinearSampler,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
};

constexpr ShaderInfo info = {
    .file = "shaders/ModelPrepass.hlsl",
    .inputLayout = gfx::CreateLayout(ModelVertex::layout),
    .rootSignature = &RootSignature
};

}

namespace neon::gfx::pipelines {
inline PipelineInfo modelPrepass = {
    .name = "model prepass",
    .shader = &shaders::ModelPrepass::info,
    .format = LINEAR_DEPTH_FORMAT,
    .blend = BlendMode::Opaque,
    .culling = CullMode::None,
    .depth = DepthMode::ReadWrite,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};
}
