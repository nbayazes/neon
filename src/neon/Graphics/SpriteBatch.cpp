#include "pch.h"
#include "SpriteBatch.h"
#include "Utility.h"
#include "CommandContext.h"
#include "Device.h"
#include "Graphics.h"
#include "ParticleSystem.h"
#include "ShaderTypes.h"

namespace neon::gfx {

void SpriteBatch::Create(uint64 capacity) {
    _capacity = capacity;
    _vertices.Create("sprite batch", sizeof(shaders::sprite::Vertex) * capacity, D3D12_HEAP_TYPE_UPLOAD);
    _textureHandles.Create("sprite batch handles", sizeof(int32) * capacity, D3D12_HEAP_TYPE_UPLOAD);

    _stagingVertexBuffer.reserve(capacity / 2);
    _stagingTextureHandles.reserve(capacity / 2);
    _sprites.reserve(capacity / 2);
}

uint SpriteBatch::Upload(bool sort) {
    auto count = (uint)_sprites.size();
    if (count == 0) return count;

    if (sort)
        Seq::sortBy(_sprites, [](auto& a, auto& b) { return a.depth < b.depth; });

    _stagingVertexBuffer.clear();
    _stagingTextureHandles.clear();

    _stagingVertexBuffer.resize(_sprites.size());
    _stagingTextureHandles.resize(_sprites.size());

    for (int i = 0; i < _sprites.size(); ++i) {
        _stagingVertexBuffer[i] = _sprites[i].vertex;
        _stagingTextureHandles[i] = _sprites[i].texture;
    }

    _vertices.Clear();
    _vertices.CopyRange(span{ _stagingVertexBuffer });

    _textureHandles.Clear();
    _textureHandles.CopyRange(span{ _stagingTextureHandles });

    vertexBufferView = {
        .BufferLocation = _vertices->GetGPUVirtualAddress(),
        .SizeInBytes = (uint)GetVectorSizeInBytes(_stagingVertexBuffer) * 4,
        .StrideInBytes = sizeof(shaders::sprite::Vertex)
    };

    textureViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    textureViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    textureViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    textureViewDesc.Buffer.FirstElement = 0;
    textureViewDesc.Buffer.NumElements = (uint)_sprites.size();
    textureViewDesc.Buffer.StructureByteStride = sizeof(int32);

    _sprites.clear();
    return count;
}

void SpriteBatcher::Draw(GraphicsContext& context, BlendMode blendMode, uint64 frame) {
    auto& batch = GetSpriteBatch(blendMode, frame);
    bool sort = RequiresSorting(blendMode);
    auto count = batch.Upload(sort);

    if (count == 0) return;

    auto cmdList = context.GetCommandList();

    switch (blendMode) {
        case BlendMode::Multiply:
            context.SetPipelineState(pipelines::spriteMultiply);
            break;
        case BlendMode::Alpha:
        case BlendMode::StraightAlpha:
            context.SetPipelineState(pipelines::sprite);
            break;
        case BlendMode::Additive:
        default:
            context.SetPipelineState(pipelines::spriteAdditive);
    }

    SetCommonShaderParameters(cmdList);
    cmdList->SetGraphicsRootShaderResourceView(3, batch.GetTextureHandleAddress());
    cmdList->SetGraphicsRootShaderResourceView(4, batch.GetVerticesAddress());

    auto& sizedResources = GetWindowSizeResources();

    sizedResources.linearDepthBuffer.Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    //cmdList->SetGraphicsRootShaderResourceView(5, sizedResources.linearDepthBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(5, sizedResources.linearDepthBuffer.GetSRV());
    //device->CreateShaderResourceView(resources.textureInfo.Get(), &resources.textureInfoView, handle.Offset(2).GetCpuHandle());

    shaders::sprite::Constants constants = {};
    constants.MultiplyMode = blendMode == BlendMode::Multiply;

    auto& frameBuffer = GetFrameBuffer();
    auto constantsOffset = frameBuffer.Copy(constants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    cmdList->SetGraphicsRootConstantBufferView(6, frameBuffer->GetGPUVirtualAddress() + constantsOffset);

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->DrawInstanced(4, count, 0, 0);
}

}
