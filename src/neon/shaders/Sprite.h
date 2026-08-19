#pragma once

#include "Graphics/ShaderTypes.h"
#include "neon-math.h"

namespace neon::gfx::shaders::sprite {

enum RootParameters : uint {
    FrameConstants, // b0
    TextureIndices, // t0
    TextureInfoTable, // t1
};

// Descriptors shared across many shaders that set up frame constants and the texture tables
constexpr D3D12_DESCRIPTOR_RANGE1 CommonDescriptors[] = {
    {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0 // b0. Frame constants
    },
    {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0, // t0. Texture info table
        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
    }
};


//constexpr D3D12_DESCRIPTOR_RANGE1 TextureHandles[] = {
//    {
//        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
//        .NumDescriptors = 1,
//        .BaseShaderRegister = 1, // t1. Texture handles
//        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
//    }
//};

constexpr D3D12_DESCRIPTOR_RANGE1 TextureTableRange = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    .NumDescriptors = UINT_MAX,
    .BaseShaderRegister = 0, // t0, space 1
    .RegisterSpace = 1,
    .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
};

constexpr D3D12_ROOT_PARAMETER1 Params[] = {
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = { .NumDescriptorRanges = std::size(CommonDescriptors), .pDescriptorRanges = CommonDescriptors },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &TextureTableRange },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
        // .DescriptorTable = { .NumDescriptorRanges = std::size(TextureHandles), .pDescriptorRanges = TextureHandles },
        .Descriptor = { .ShaderRegister = 1 }, // Texture handles, t1
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
        // .DescriptorTable = { .NumDescriptorRanges = std::size(TextureHandles), .pDescriptorRanges = TextureHandles },
        .Descriptor = {.ShaderRegister = 2 }, // Vertices, t2
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
    },
};

constexpr D3D12_STATIC_SAMPLER_DESC StaticLinearSampler{
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
};

constexpr D3D12_ROOT_SIGNATURE_DESC1 RootSignature = {
    .NumParameters = std::size(Params),
    .pParameters = Params,
    .NumStaticSamplers = 1,
    .pStaticSamplers = &StaticLinearSampler,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
};

struct Vertex {
    Vector3 position;
    Color color;
    Vector2 size;

    static constexpr D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
};

constexpr ShaderInfo info = {
    .file = "shaders/Sprite.hlsl",
    .inputLayout = gfx::CreateLayout(Vertex::layout),
    .rootSignature = &RootSignature
};

}

namespace neon::gfx::pipelines {

inline PipelineInfo sprite = {
    .name = "sprite",
    .shader = &shaders::sprite::info,
    .format = DXGI_FORMAT_R11G11B10_FLOAT,
    .blend = BlendMode::Alpha,
    .culling = CullMode::Clockwise,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};

inline PipelineInfo spriteAdditive = {
    .name = "sprite additive",
    .shader = &shaders::sprite::info,
    .format = DXGI_FORMAT_R11G11B10_FLOAT,
    .blend = BlendMode::Additive,
    .culling = CullMode::Clockwise,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};

}
