#pragma once

#include "Graphics/ShaderTypes.h"
#include "neon-math.h"

namespace neon::gfx::shaders::model {

enum RootParameters : uint {
    RootConstants,
    Sampler,
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

// todo: these should be pushed to a single contiguous buffer for MDI
struct Constants {
    Matrix world;
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
};

}

namespace neon::gfx::pipelines {

inline PipelineInfo model = {
    .name = "model",
    .shader = &shaders::model::info,
    .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .blend = BlendMode::Opaque, // Alpha?
    .culling = CullMode::None,
    .depth = DepthMode::Read,
    .stencil = StencilMode::PortalRead,
    .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
};

// prepass
}
