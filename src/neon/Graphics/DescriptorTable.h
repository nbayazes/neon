#pragma once
#include <mutex>
#include "GpuResource.h"
#include "Descriptor.h"

namespace neon::gfx {
    // there are four types of descriptor heaps
    class DescriptorHeap {
        ID3D12Device* _device;
        D3D12_DESCRIPTOR_HEAP_DESC _desc = {};
        uint32 _descriptorSize = 0;
        ComPtr<ID3D12DescriptorHeap> _heap;
        DescriptorHandle _start = {};
        //uint32 _index = 0;

        uint32 _reserved = 0;

    public:
        DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint size, string_view name) : _device(device) {
            bool shaderVisible = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

            _desc = {
                .Type = type,
                .NumDescriptors = size,
                .Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                .NodeMask = 1
            };

            ThrowIfFailed(device->CreateDescriptorHeap(&_desc, IID_PPV_ARGS(_heap.ReleaseAndGetAddressOf())));
            _descriptorSize = device->GetDescriptorHandleIncrementSize(_desc.Type);
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = shaderVisible ? _heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
            _start = { _heap->GetCPUDescriptorHandleForHeapStart(), gpuHandle, _descriptorSize };
            SetName(_heap, name);
        }

        auto Size() const { return _desc.NumDescriptors; }
        auto Heap() const { return _heap.Get(); }
        auto DescriptorSize() const { return _start.DescriptorSize(); }
        auto Type() const { return _desc.Type; }

        //uint32 Allocate() {
        //    return _index++;
        //}

        // Reserves a section of the descriptor heap. Returns heap offset.
        uint32 Reserve(uint32 descriptors) {
            auto offset = _reserved;
            _reserved += descriptors;

            // Reserve all remaining
            if (descriptors == UINT32_MAX) {
                _reserved = _desc.NumDescriptors;
                return _desc.NumDescriptors - _reserved;
            }

            if (_reserved > _desc.NumDescriptors)
                throw Exception("Descriptor heap is out of space!");

            return offset;
        }

        DescriptorHandle GetHandle(uint index) const {
            if (index >= Size())
                throw Exception("Descriptor handle index is out of range");

            return _start.Offset(index);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE AddSRV(GpuResource& resource, uint32 index) const {
            ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            resource._srv = GetHandle(index);
            auto desc = resource.GetSrvDesc();
            _device->CreateShaderResourceView(resource.Get(), &desc, resource._srv.GetCpuHandle());
            return resource.GetSRV();
        }

        D3D12_GPU_DESCRIPTOR_HANDLE AddUAV(GpuResource& resource, uint32 index) const {
            ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            //auto desc = useDefaultDesc ? nullptr : &_uavDesc;
            resource._uav = GetHandle(index);
            auto desc = resource.GetUavDesc();
            _device->CreateUnorderedAccessView(resource.Get(), nullptr, &desc, resource._uav.GetCpuHandle());
            return resource.GetUAV();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE AddRTV(GpuResource& resource, uint index) const;

        D3D12_CPU_DESCRIPTOR_HANDLE AddDSV(GpuResource& resource, uint index) const {
            ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            //auto desc = useDefaultDesc ? nullptr : &_uavDesc;
            resource._dsv = GetHandle(index);
            auto desc = resource.GetDsvDesc();
            _device->CreateDepthStencilView(resource.Get(), &desc, resource._dsv.GetCpuHandle());
            return resource.GetDSV();
        }

        //void AddCBV(GpuResource& resource) {
        //    ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //    _device->CreateConstantBufferView(desc, _uav.GetCpuHandle());
        //}
    };

    // A descriptor range sequentially allocates descriptors until it runs out of space
    class DescriptorRange {
        DescriptorHeap* _heap = nullptr;
        uint _offset = 0; // offset into the heap
        uint _descriptors = 0; // number of descriptors
        std::mutex _indexLock;
        D3D12MA::VirtualBlock* _block = nullptr;
        uint _count = 0; // number of allocated descriptors

    public:
        DescriptorRange(DescriptorHeap* heap, uint descriptors = UINT32_MAX)
            : _heap(heap), _descriptors(descriptors) {
            _offset = _heap->Reserve(descriptors);
            //SPDLOG_INFO("Created descriptor range with offset: {} and size: {}", offset, descriptors);

            D3D12MA::VIRTUAL_BLOCK_DESC blockDesc = {};
            blockDesc.Size = descriptors;
            blockDesc.Flags = D3D12MA::VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR;
            ThrowIfFailed(CreateVirtualBlock(&blockDesc, &_block));
        }

        void Clear() {
            _block->Clear();
            _count = 0;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE AddSRV(GpuResource& resource) {
            return _heap->AddSRV(resource, Next() + _offset);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE AddUAV(GpuResource& resource) {
            return _heap->AddUAV(resource, Next() + _offset);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE AddRTV(GpuResource& resource) {
            return _heap->AddRTV(resource, Next() + _offset);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE AddDSV(GpuResource& resource) {
            return _heap->AddDSV(resource, Next() + _offset);
        }

        // Returns the next available descriptor index
        uint32 Next() {
            std::scoped_lock lock(_indexLock);

            D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
            allocDesc.Size = 1;

            D3D12MA::VirtualAllocation alloc;
            uint64 offset;
            ThrowIfFailed(_block->Allocate(&allocDesc, &alloc, &offset));
            _count++;

            return (uint32)offset;
        }

        uint32 Count() const { return _count; }

        DescriptorHandle GetHandle(uint index) const { return _heap->GetHandle(_offset + index); }
        DescriptorHandle GetNextHandle() { return _heap->GetHandle(_offset + Next()); }
        DescriptorHandle operator[](int index) const { return GetHandle(index); }

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint index = 0) const {
            return _heap->GetHandle(_offset + index).GetGpuHandle();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint index = 0) const {
            return _heap->GetHandle(_offset + index).GetCpuHandle();
        }

        //size_t GetSize() const { return _descriptors; }
    };
}
