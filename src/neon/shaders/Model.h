#pragma once

#include "Graphics/ShaderTypes.h"
#include "neon-math.h"

namespace neon::gfx::shaders::model {

enum RootParameters : uint {
    FrameConstants, // b0
    ObjectConstants, // b1
    //TextureTable, // t0, space1
};

enum RootSamplers : uint {
    Sampler, // s0
    NormalSampler, // s1
};

//FrameConstants, // b0
//TextureTable, // t0, space1
//RootConstants, // b1
//Matcap, // t0
//MaterialInfoBuffer, // t5
//VClipTable, // t6
//DissolveTexture, // t7
//EnvironmentCube, // t8
//Sampler, // s0
//NormalSampler, // s1
//LightGrid, // t11, t12, t13, b2

constexpr D3D12_DESCRIPTOR_RANGE1 TextureTableRange = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    .NumDescriptors = UINT_MAX,
    .BaseShaderRegister = 0, // t0
    .RegisterSpace = 1,
    .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
};

constexpr D3D12_DESCRIPTOR_RANGE1 DiffuseSamplerDescriptor = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
    .NumDescriptors = 1,
    .BaseShaderRegister = Sampler
};

constexpr D3D12_DESCRIPTOR_RANGE1 NormalSamplerDescriptor = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
    .NumDescriptors = 1,
    .BaseShaderRegister = NormalSampler
};

constexpr D3D12_ROOT_PARAMETER1 Params[] = {
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
        .Descriptor = { .ShaderRegister = FrameConstants },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
        .Descriptor = { .ShaderRegister = ObjectConstants },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = &TextureTableRange },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = &DiffuseSamplerDescriptor },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    },
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = &NormalSamplerDescriptor },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    }
};

constexpr D3D12_ROOT_SIGNATURE_DESC1 RootSignature = {
    .NumParameters = std::size(Params),
    .pParameters = Params,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
};

struct Constants {
    Matrix world = Matrix::Identity;
};

struct Vertex {
    Vector3 position;
    Vector2 uv;
    Color color;
    Vector3 normal;
    Vector3 tangent;
    Vector3 bitangent;

    static constexpr D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
};

//constexpr void SetProjectionMatrix(ID3D12GraphicsCommandList* commandList, const Arguments& args) {
//    commandList->SetGraphicsRoot32BitConstants(RootParameters, sizeof(args) / 4, &args, 0);
//}

constexpr void SetSampler(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE sampler) {
    commandList->SetGraphicsRootDescriptorTable(Sampler, sampler);
}

static void SetConstants(ID3D12GraphicsCommandList* commandList, const Constants& consts) {
    //Render::BindTempConstants(commandList, consts, RootConstants);
}

constexpr ShaderInfo info = {
    .file = "shaders/model.hlsl",
    .inputLayout = gfx::CreateLayout(Vertex::layout),
    .rootSignature = &RootSignature
};

}

namespace neon::gfx::pipelines {

inline PipelineInfo model = {
    .name = "model",
    .shader = &shaders::model::info,
    .format = DXGI_FORMAT_R11G11B10_FLOAT,
    .blend = BlendMode::Opaque, // Alpha?
    .culling = CullMode::None,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};

// prepass
}
