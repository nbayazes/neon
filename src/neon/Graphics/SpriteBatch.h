#pragma once
#include "GpuResource.h"
#include "GraphicsHandles.h"
#include "shaders/Sprite.h"

namespace neon::gfx {
class GraphicsContext;

struct SpriteBatchInfo {
    shaders::sprite::Vertex vertex;
    float depth = 0;
    TexID texture = TexID::None;
};

class SpriteBatch {
    List<SpriteBatchInfo> _sprites;
    GpuBuffer _vertices;
    GpuBuffer _textureHandles;

    List<shaders::sprite::Vertex> _stagingVertexBuffer;
    List<TexID> _stagingTextureHandles;
    uint64 _capacity = 0;

public:
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC textureViewDesc = {};

    void Create(uint64 capacity);

    void Add(const SpriteBatchInfo& sprite) {
        if (_sprites.size() + 1 >= _capacity) return; // prevent overflow
        _sprites.push_back(sprite);
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetTextureHandleAddress() {
        return _textureHandles->GetGPUVirtualAddress();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetVerticesAddress() {
        return _vertices->GetGPUVirtualAddress();
    }

    //void Clear() {
    //    _sprites.clear();
    //}

    //void Sort();

    //span<SpriteBatchInfo> Sprites() { return _sprites; }

    // Returns the number of sprites that were uploaded
    uint Upload(bool sort);
};


class SpriteBatcher {
    List<SpriteBatch> _alphaBatch;
    List<SpriteBatch> _additiveBatch;
    List<SpriteBatch> _multiplyBatch;
    uint _backBufferCount = 0;

public:
    void Initialize(uint backBufferCount, uint capacity) {
        _backBufferCount = backBufferCount;
        ASSERT(backBufferCount < 4);
        _alphaBatch.resize(backBufferCount);
        _additiveBatch.resize(backBufferCount);
        _multiplyBatch.resize(backBufferCount);

        for (uint i = 0; i < backBufferCount; ++i) {
            _alphaBatch[i].Create(capacity);
            _additiveBatch[i].Create(capacity);
            _multiplyBatch[i].Create(capacity);
        }
    }

    SpriteBatch& GetSpriteBatch(BlendMode mode, uint64 frame) {
        switch (mode) {
            case BlendMode::Alpha: return _alphaBatch[frame % _backBufferCount];
            case BlendMode::Multiply: return _multiplyBatch[frame % _backBufferCount];
            default: return _additiveBatch[frame % _backBufferCount];
        }
    }

    void Draw(GraphicsContext& context, BlendMode blendMode, uint64 frame);
};

}
