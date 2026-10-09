#include "pch.h"
#include "SpriteBatch.h"
#include "Utility.h"

namespace neon::gfx {

void SpriteBatch::Create(uint64 capacity) {
    _capacity = capacity;
    _vertices.Create("sprite batch", sizeof(shaders::sprite::Vertex) * capacity, D3D12_HEAP_TYPE_UPLOAD);
    _textureHandles.Create("sprite batch handles", sizeof(int32) * capacity, D3D12_HEAP_TYPE_UPLOAD);

    _stagingVertexBuffer.reserve(capacity / 2);
    _stagingTextureHandles.reserve(capacity / 2);
    _sprites.reserve(capacity / 2);
}

//void SpriteBatch::Sort() {
//    Seq::sortBy(_sprites, [](auto& a, auto& b) { return a.depth < b.depth; });
//}

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

}
