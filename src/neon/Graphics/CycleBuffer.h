#pragma once

#include "CommandContext.h"
#include "GpuResource.h"
#include "DeviceResources.h"

namespace neon::gfx {

//D3D12MA::Allocator* GetMemoryAllocator();
//ID3D12Device* GetDevice();

// A cycle buffer keeps multiple copies of a type in a buffer without having to be concerned about frames in flight.
//
// https://moonside.games/posts/sdl-gpu-concepts-cycling/
//template <typename T>
//class CycleBuffer : public GpuResource {
//    uint _elementCount = 0;
//    D3D12MA::VirtualBlock* _block = nullptr;
//    gfx::GpuBuffer _uploadBuffer;
//    //Ptr<gfx::CommandContext> _uploadContext;
//    List<D3D12MA::VirtualAllocation> _allocations;
//    uint _activeSlot = 0;
//
//public:
//    void Create(const string& name, uint slots) {
//        _allocations.resize(slots);
//        auto size = slots * sizeof(T);
//        _desc = CD3DX12_RESOURCE_DESC::Buffer(size);
//
//        //_uploadBuffer.Create(name + " upload buffer", sizeof(T), D3D12_HEAP_TYPE_UPLOAD);
//
//        D3D12MA::ALLOCATION_DESC allocDesc = { .HeapType = D3D12_HEAP_TYPE_DEFAULT };
//        auto allocator = GetMemoryAllocator();
//
//        ThrowIfFailed(allocator->CreateResource(
//            &allocDesc,
//            &_desc,
//            _state,
//            nullptr,
//            _allocation.ReleaseAndGetAddressOf(),
//            IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
//        ));
//
//        D3D12MA::VIRTUAL_BLOCK_DESC blockDesc = {};
//        blockDesc.Size = size;
//        blockDesc.Flags = D3D12MA::VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR;
//        ThrowIfFailed(CreateVirtualBlock(&blockDesc, &_block));
//
//        SetName(_resource, name);
//        //_uploadContext = make_unique<gfx::CommandContext>(GetDevice(), GetDeviceResources().copyQueue.get(), name + " command context");
//    }
//
//    // Copies data to the buffer and increments the active resource
//    UINT64 Copy(T& data, uint64 alignment = 4) {
//        D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
//        allocDesc.Size = sizeof(T);
//        allocDesc.Alignment = alignment;
//
//        _activeSlot++;
//        if(_allocations[_activeSlot].AllocHandle) {
//            _block->FreeAllocation(_allocations.front());
//        }
//
//        UINT64 offset;
//        ThrowIfFailed(_block->Allocate(&allocDesc, &_allocations[_activeSlot], &offset));
//        //memcpy(_mappedPtr + offset, &data, allocDesc.Size);
//
//        return offset;
//    }
//
//protected:
//    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
//        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
//        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//        desc.Format = DXGI_FORMAT_UNKNOWN;
//        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//        desc.Buffer.NumElements = _elementCount;
//        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
//        return desc;
//    }
//};

}
