#pragma once
#include "neon-types.h"
#include "neon-graphics.h"

namespace neon::gfx {
    constexpr D3D12_INPUT_LAYOUT_DESC CreateLayout(span<const D3D12_INPUT_ELEMENT_DESC> desc) {
        return { desc.data(), (uint)desc.size() };
    }

    struct ShaderInfo {
        string_view file;
        string_view vsEntryPoint = "vsmain";
        string_view psEntryPoint = "psmain";
        D3D12_INPUT_LAYOUT_DESC inputLayout{};
        // For code defined root signatures. If null the signature is read from the HLSL file.
        const D3D12_ROOT_SIGNATURE_DESC1* rootSignature = nullptr;
    };

    enum class BlendMode { Opaque, Alpha, StraightAlpha, Additive, Multiply }; // "Alpha" is premultiplied
    enum class CullMode { None, CounterClockwise, Clockwise, Wireframe };
    enum class DepthMode { ReadWrite, Read, ReadDecalBiased, ReadSpriteBiased, ReadEqual, None };
    enum class StencilMode { None, PortalRead, PortalReadNeq, PortalWrite };

    struct PipelineInfo {
        string_view name; // debug name
        const ShaderInfo* shader = nullptr;
        DXGI_FORMAT format = DXGI_FORMAT_R11G11B10_FLOAT;
        BlendMode blend = BlendMode::Opaque;
        CullMode culling = CullMode::CounterClockwise;
        DepthMode depth = DepthMode::ReadWrite;
        StencilMode stencil = StencilMode::None;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bool enableMultisample = true; // enables multisampling if the current settings allow it

        // compiled state. it would be better for this to be stored elsewhere, but all alternatives make it less ergonomic
        ID3D12PipelineState* pso = nullptr;
        ID3D12RootSignature* rootSignature = nullptr;
    };

    struct CompiledShader {
        const ShaderInfo* info = nullptr;
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        ComPtr<ID3D12RootSignature> rootSignature;
    };

    struct CompiledPipeline {
        ComPtr<ID3D12PipelineState> pso;
        PipelineInfo* info = nullptr;
        ID3D12RootSignature* rootSignature = nullptr;
    };
}
