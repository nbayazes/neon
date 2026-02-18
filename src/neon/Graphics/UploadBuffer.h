#pragma once

#include "Descriptor.h"
#include "neon-graphics.h"
#include "neon-types.h"
#include "Logging.h"

namespace neon::gfx {
    constexpr D3D12_RANGE CPU_READ_NONE = {};
    //inline const D3D12_RANGE* CPU_READ_ALL = nullptr;

    // Creates a buffer upload heap
    void CreateUploadHeap(ComPtr<ID3D12Resource>& resource, uint64 bufferSize);

    // Resizable buffer that uses the upload heap every frame.
    // intended for use with small dynamic buffers
    template <class T>
    class UploadBuffer {
        ComPtr<ID3D12Resource> _resource;
        bool _inUpdate = false;
        T* _mappedData = nullptr;
        size_t _gpuCapacity = 0, _requestedCapacity, _gpuElements = 0;
        List<T> _buffer;
        DescriptorHandle _srv, _uav;
        string _name;

    public:
        UploadBuffer(size_t capacity, string_view name) : _requestedCapacity(capacity), _name(name) {
            _buffer.reserve(capacity);
            _gpuCapacity = _requestedCapacity;

            CreateUploadHeap(_resource, _gpuCapacity * sizeof(T));
            SetName(_resource, name);

            //if (_mapped) _resource->Unmap(0, &CPU_READ_NONE);
            // leave the buffer mapped
            ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedData));
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return _resource->GetGPUVirtualAddress(); }
        uint GetSizeInBytes() const { return (uint)(sizeof(T) * _gpuCapacity); }
        uint GetElementCount() const { return (uint)_gpuElements; }
        static uint GetStride() { return sizeof(T); }

        D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return _srv.GetGpuHandle(); }
        D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return _uav.GetGpuHandle(); }

        ID3D12Resource* Get() const { return _resource.Get(); }

        //void CreateShaderResourceView() {
        //    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        //    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        //    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        //    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //    srvDesc.Buffer.NumElements = (UINT)_buffer.size();
        //    srvDesc.Buffer.StructureByteStride = GetStride();
        //    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //    if (!_srv) _srv = Render::Descriptors->reserved.Allocate();
        //    Render::Device->CreateShaderResourceView(_resource.Get(), &srvDesc, _srv.GetCpuHandle());
        //}

        //void CreateUnorderedAccessView() {
        //    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        //    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        //    desc.Format = DXGI_FORMAT_UNKNOWN;
        //    desc.Buffer.CounterOffsetInBytes = 0;
        //    desc.Buffer.NumElements = _buffer.size();
        //    desc.Buffer.StructureByteStride = Stride;
        //    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        //    if (!_uav) _uav = Render::Heaps->Reserved.Allocate();
        //    Render::Device->CreateUnorderedAccessView(_resource.Get(), nullptr, &desc, _uav.GetCpuHandle());
        //}

        void Begin() {
            if (_inUpdate) throw Exception("Already called Begin");
            _inUpdate = true;

            bool shouldGrow = _requestedCapacity > _gpuCapacity;
            if (!_resource || shouldGrow) {
                if (shouldGrow)
                    _gpuCapacity = size_t(_requestedCapacity * 1.5);
                CreateUploadHeap(_resource, _gpuCapacity * sizeof(T));
                std::ignore = _resource->SetName(Widen(_name).c_str());

                //if (_mapped) _resource->Unmap(0, &CPU_READ_NONE);
                // leave the buffer mapped
                ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedData));
            }

            _buffer.clear();
        }

        uintptr_t Copy(span<const T> src) {
            if (!_inUpdate)
                throw Exception("Must call Begin before Copy");

            auto offset = _buffer.size();
            if (_buffer.size() + src.size() > _gpuCapacity) {
                _requestedCapacity = _buffer.size() + src.size(); // ran out of space
                return 0;
            }

            _buffer.insert(_buffer.end(), src.begin(), src.end());
            return offset;
        }

        bool End() {
            if (!_inUpdate)
                throw Exception("Must call Begin before End");

            _inUpdate = false;

            // copy to GPU
            memcpy(_mappedData, _buffer.data(), _buffer.size() * sizeof(T));
            _gpuElements = _buffer.size();
            return true;
        }

        uintptr_t IncrementalAppend(span<const T> src) {
            auto offset = _buffer.size();
            if (_buffer.size() + src.size() > _gpuCapacity) {
                __debugbreak();
                _requestedCapacity = _buffer.size() + src.size(); // ran out of space
                return 0;
            }

            _buffer.insert(_buffer.end(), src.begin(), src.end());

            memcpy(_mappedData + offset, src.data(), src.size() * sizeof(T));
            _gpuElements += _buffer.size();

            return offset;
        }
    };

    enum class ChunkHandle: size_t;

    // Generic upload buffer.
    // Internally splits memory into blocks and allocates chunks based on the requested amount.
    class GenericBuffer {
        ComPtr<ID3D12Resource> _resource;
        ubyte* _mappedData = nullptr;
        DescriptorHandle _srv, _uav;
        size_t _size; // total size
        size_t _blockSize; // size of one block
        string _name;
        List<ubyte> _free; // block freelist
        std::mutex _mutex;

        struct ChunkAllocation {
            size_t startBlock = 0; // chunk start in blocks
            size_t chunks = 0; // number of chunks this allocation uses
            size_t size = 0; // total bytes
        };

        List<ChunkAllocation> _allocations;

    public:
        GenericBuffer(size_t blocks, size_t blockSize, string_view name)
            : _size(blocks * blockSize), _blockSize(blockSize), _name(name) {
            CreateUploadHeap(_resource, _size);
            SetName(_resource, name);

            _free.resize(blocks);
            ranges::fill(_free, true);

            // leave the buffer mapped
            ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedData));
        }

        // Returns a handle to the allocation
        template <class T>
        ChunkHandle Copy(span<const T> src) {
            std::scoped_lock lock(_mutex);
            auto size = src.size() * sizeof(T);
            auto handle = Allocate(size);
            auto& alloc = _allocations[(size_t)handle];

            memcpy(_mappedData + alloc.startBlock * _blockSize, src.data(), size);
            return handle;
        }

        void Free(ChunkHandle index) {
            std::scoped_lock lock(_mutex);
            auto idx = (size_t)index;
            if (idx >= _allocations.size()) return;
            auto& alloc = _allocations[idx];
            
            for (size_t i = 0; i < alloc.chunks; ++i) {
                _free[alloc.startBlock + i] = true;
            }

            SPDLOG_INFO("Freeing block {}", (size_t)index);
            memset(_mappedData + alloc.startBlock * _blockSize, 0, alloc.size);

            _allocations.erase(_allocations.begin() +  (ptrdiff_t)index);
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(ChunkHandle index) const {
            auto idx = (size_t)index;
            if (idx >= _allocations.size()) return{};
            auto& alloc = _allocations[idx];
            return _resource->GetGPUVirtualAddress() + alloc.startBlock * _blockSize;
        }

    private:
        // Returns the index of the allocation record
        ChunkHandle Allocate(size_t size) {
            const auto requiredChunks = (size_t)std::ceil((float)size / (float)_blockSize);

            // todo: this could be improved to track the starting free index
            for (size_t i = 0; i < _free.size(); ++i) {
                if (!_free[i]) continue;

                if (requiredChunks > 1) {
                    if (i + requiredChunks >= _free.size())
                        throw Exception("Not enough space in buffer!");

                    // check for enough consecutive required chunks
                    bool okay = true;

                    for (size_t next = 1; next <= requiredChunks; next++) {
                        if (!_free[i + next]) {
                            //i += next; // skip over checked blocks
                            okay = false;
                            break;
                        }
                    }

                    if (!okay) continue;
                }

                for (size_t c = 0; c < requiredChunks; ++c) {
                    _free[i + c] = false;
                }

                SPDLOG_INFO("Allocating {} chunk(s) at {}. {} bytes", requiredChunks, i, size);

                auto& alloc = _allocations.emplace_back();

                alloc = {
                    .startBlock = i,
                    .chunks = requiredChunks,
                    .size = size
                };

                return ChunkHandle(_allocations.size() - 1);
            }

            throw Exception("No free chunks in buffer!");
        }
    };
}
