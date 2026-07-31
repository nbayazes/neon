#pragma once

#include <d3d12.h>
#include "neon-types.h"
#include "Utility.h"

namespace neon::gfx {
    // Packs multiple vertex and index buffers into a single buffer.
    // Packed buffers use GraphicsResource which automatically upload at end of frame.
    class PackedBuffer {
        uint _index = 0;
        uint _size;
        DirectX::GraphicsResource _resource;

    public:
        PackedBuffer(uint size = 1024 * 1024 * 20)
            : _size(size) {
            _resource = DirectX::GraphicsMemory::Get().Allocate(size);
        }

        void ResetIndex() { _index = 0; }

        // Adds vertices to the buffer and returns a view at the start of the data
        template <class TVertex>
        D3D12_VERTEX_BUFFER_VIEW PackVertices(span<TVertex> data) {
            constexpr auto stride = sizeof(TVertex);
            auto size = uint(data.size() * stride);
            if (_index + size > _size)
                throw Exception("Ran out of space in GPU buffer");

            memcpy((byte*)_resource.Memory() + _index, data.data(), size);

            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = _resource.GpuAddress() + _index;
            vbv.SizeInBytes = size;
            vbv.StrideInBytes = stride;

            _index += size;
            _index = AlignTo(_index, 4); // alignment of 4 to prevent issues on AMD
            return vbv;
        }

        // Adds indices to the buffer and returns a view at the start of the data
        template <class TIndex = uint16>
        D3D12_INDEX_BUFFER_VIEW PackIndices(span<TIndex> data) {
            constexpr auto stride = sizeof(TIndex);
            static_assert(stride == 2 || stride == 4);
            constexpr auto format = stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

            auto size = uint(data.size() * stride);
            if (_index + size > _size)
                throw Exception("Ran out of space in GPU buffer");

            memcpy((byte*)_resource.Memory() + _index, data.data(), size);

            D3D12_INDEX_BUFFER_VIEW ibv{};
            ibv.BufferLocation = _resource.GpuAddress() + _index;
            ibv.SizeInBytes = size;
            ibv.Format = format;
            _index += size;
            _index = AlignTo(_index, 4); // alignment of 4 to prevent issues on AMD
            return ibv;
        }
    };
}