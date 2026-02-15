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
            auto dest = GetHandle(index);
            auto desc = resource.GetUavDesc();
            _device->CreateUnorderedAccessView(resource.Get(), nullptr, &desc, dest.GetCpuHandle());
            return resource.GetUAV();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE AddRTV(GpuResource& resource, uint index) const;

        D3D12_CPU_DESCRIPTOR_HANDLE AddDSV(GpuResource& resource, uint index) const {
            ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            //auto desc = useDefaultDesc ? nullptr : &_uavDesc;
            auto dest = GetHandle(index);
            auto desc = resource.GetDsvDesc();
            _device->CreateDepthStencilView(resource.Get(), &desc, dest.GetCpuHandle());
            return resource.GetDSV();
        }

        //void AddCBV(GpuResource& resource) {
        //    ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //    _device->CreateConstantBufferView(desc, _uav.GetCpuHandle());
        //}
    };

    class LinearDescriptorRange {
        DescriptorHeap* _heap = nullptr;
        uint _offset = 0; // offset into the heap
        uint _descriptors = 0; // number of descriptors
        uint _index = 0;
        std::mutex _indexLock;

    public:
        LinearDescriptorRange(DescriptorHeap* heap, uint descriptors = UINT32_MAX)
            : _heap(heap), _descriptors(descriptors) {
            _offset = _heap->Reserve(descriptors);
            //SPDLOG_INFO("Created descriptor range with offset: {} and size: {}", offset, descriptors);
        }

        // Allows descriptors to wrap back to the start of the range
        bool allowWrapping = false;

        void ResetIndex() {
            _index = 0;
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

        // Returns the next descriptor index. Wraps if out of space.
        uint Next() {
            std::scoped_lock lock(_indexLock);

            if (_index + 1 >= _descriptors) {
                if (allowWrapping) {
                    _index = 0;
                    return 0;
                } else {
                    throw Exception("Out of space in descriptor range");
                }
            }

            return _index++;
        }

        //DescriptorHandle Allocate() {
        //    return GetHandle(Next());
        //}

        DescriptorHandle GetHandle(uint index) const { return _heap->GetHandle(_offset + index); }
        DescriptorHandle operator[](int index) const { return GetHandle(index); }

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint index = 0) const {
            return _heap->GetHandle(_offset + index).GetGpuHandle();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint index = 0) const {
            return _heap->GetHandle(_offset + index).GetCpuHandle();
        }

        size_t GetSize() const { return _descriptors; }
    };

    /*
     * A descriptor table partitions a descriptor heap into separate addressable ranges.
     *
     * Ranges can be reset individually and passed to shaders as root descriptor tables.
     */
    //class DescriptorTable {
    //    D3D12_DESCRIPTOR_HEAP_DESC _desc = {};
    //    ID3D12DescriptorHeap* _heap;
    //    DescriptorHandle _start = {};
    //    uint32 _descriptorSize = 0;
    //    uint _index = 0;
    //    std::mutex _indexLock;

    //public:
    //    DescriptorTable(ID3D12DescriptorHeap& heap) : _heap(&heap) {  }
    //    uint Size() const { return _desc.NumDescriptors; }
    //    ID3D12DescriptorHeap* Heap() const { return _heap; }
    //    //uint DescriptorSize() const { return _start.DescriptorSize(); }
    //    //D3D12_DESCRIPTOR_HEAP_TYPE Type() const { return _desc.Type; }

    //    // Gets a specific handle by index.
    //    DescriptorHandle GetHandle(uint index) const {
    //        if (index >= Size())
    //            throw Exception("Descriptor handle index is out of range");

    //        return _start.Offset(index);
    //    }

    //    DescriptorHandle operator[](uint index) const { return GetHandle(index); }

    //    void SetName(string_view name) const {
    //        ThrowIfFailed(_heap->SetName(Widen(name).c_str()));
    //    };

    //    // Returns an unused handle. This ignores any direct index usage.
    //    //DescriptorHandle Allocate(uint count = 1) {
    //    //    std::scoped_lock lock(_indexLock);
    //    //    auto index = _index;
    //    //    _index += count;
    //    //    return GetHandle((int)index);
    //    //}

    //    class Range {
    //        friend DescriptorTable;

    //        DescriptorTable* _heap = nullptr;
    //        uint _offset = 0; // offset into the heap
    //        uint _descriptors = 0; // number of descriptors
    //        uint _index = 0;
    //        std::mutex _indexLock;

    //        Range(DescriptorTable* heap, uint descriptors, uint offset)
    //            : _heap(heap), _offset(offset), _descriptors(descriptors) {
    //            //SPDLOG_INFO("Created descriptor range with offset: {} and size: {}", offset, descriptors);
    //        }

    //    public:
    //        Range() = default;

    //        void Reset() {
    //            _index = 0;
    //        }

    //        //void AddSRV(GpuResource& resource) {
    //        //    if (!_srv) _srv = Render::Descriptors->reserved.Allocate();
    //        //    Render::Device->CreateShaderResourceView(Get(), &_srvDesc, _srv.GetCpuHandle());
    //        //}

    //        //void AddUAV(GpuResource& resource) {
    //        //    // UAVs are mixed with SRVs
    //        //    auto desc = useDefaultDesc ? nullptr : &_uavDesc;
    //        //    Render::Device->CreateUnorderedAccessView(Get(), nullptr, desc, _uav.GetCpuHandle());
    //        //}

    //        //void AddRTV(GpuResource& resource) {
    //        //    if (!_rtv) _rtv = Render::RenderTargetDescriptors->Allocate();
    //        //    Render::Device->CreateRenderTargetView(Get(), &_rtvDesc, _rtv.GetCpuHandle());
    //        //}

    //        // Returns the next descriptor index
    //        uint Next() {
    //            std::scoped_lock lock(_indexLock);
    //            if (_index + 1 >= _descriptors) {
    //                //SPDLOG_ERROR("No free indices in descriptor range!");
    //                return 0;
    //            }
    //            return _index++;
    //        }

    //        DescriptorHandle Allocate() {
    //            return GetHandle(Next());
    //        }

    //        DescriptorHandle GetHandle(uint index) const { return _heap->GetHandle(_offset + index); }
    //        DescriptorHandle operator[](int index) const { return GetHandle(index); }

    //        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint index = 0) const {
    //            return _heap->GetHandle(_offset + index).GetGpuHandle();
    //        }

    //        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint index = 0) const {
    //            return _heap->GetHandle(_offset + index).GetCpuHandle();
    //        }

    //        size_t GetSize() const { return _descriptors; }
    //    };

    //    Range CreateRange(uint descriptors) {
    //        ASSERT(_index + descriptors <= Size());
    //        auto index = _index;
    //        _index += descriptors;
    //        return { this, descriptors, index };
    //    }
    //};
}
