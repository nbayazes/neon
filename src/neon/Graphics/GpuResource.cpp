#include "pch.h"
#include "GpuResource.h"
#include "DeviceResources.h"
#include "neon.h"
#include "neon-strings.h"

namespace neon::gfx {

void GpuResource::Create(D3D12_HEAP_TYPE heapType, string_view name, const D3D12_CLEAR_VALUE* clearValue, bool forceComitted) {
    D3D12MA::ALLOCATION_DESC allocDesc = { .HeapType = heapType };
    if (forceComitted) allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED; // procedurals are running into an issue when copying resources to aliased textures

    // Enable aliasing on small textures (this is done automatically by the allocator)
    // Placed resources save memory as long as the resolution is 64x64
    if (!forceComitted && _desc.Width <= 128 && _desc.Height <= 128) {
        allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_CAN_ALIAS;
    }

    auto allocator = GetMemoryAllocator();

    ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &_desc,
        _state,
        clearValue,
        _allocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
    ));

    SetName(_resource, name);
}

void GpuBuffer::Create(string_view name, uint64 size, D3D12_HEAP_TYPE heapType) {
    _desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    _heapType = heapType;
    _name = name;

    if (size == 0) {
        __debugbreak();
        return;
    }

    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
        _state = D3D12_RESOURCE_STATE_GENERIC_READ;

    // todo: pool should have MinBlockCount set to 1 due to ring buffer
    //D3D12MA::POOL_DESC pool = {
    //    .Flags = D3D12MA_RECOMMENDED_HEAP_FLAGS,
    //    .HeapProperties = ,
    //    .HeapFlags = ,
    //    .BlockSize = ,
    //    .MinBlockCount = 1,
    //    .MaxBlockCount = ,
    //    .MinAllocationAlignment = ,
    //    .pProtectedSession = ,
    //    .ResidencyPriority = 
    //};

    D3D12MA::ALLOCATION_DESC allocDesc = { .HeapType = heapType };
    auto allocator = GetMemoryAllocator();

    ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &_desc,
        _state,
        nullptr,
        _allocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
    ));

    D3D12MA::VIRTUAL_BLOCK_DESC blockDesc = {};
    blockDesc.Size = size;
    blockDesc.Flags = D3D12MA::VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR;
    ThrowIfFailed(CreateVirtualBlock(&blockDesc, &_block));

    SetName(_resource, name);

    if(_heapType == D3D12_HEAP_TYPE_UPLOAD) {
        Map();
        //ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedPtr));
    }
}

IntermediateResource::IntermediateResource(const D3D12_RESOURCE_DESC& desc) {
    D3D12MA::ALLOCATION_DESC heapDesc{ .HeapType = D3D12_HEAP_TYPE_UPLOAD };

    UINT64 uploadBufferSize;
    neon::gfx::GetDevice()->GetCopyableFootprints(&desc, 0, desc.MipLevels * desc.DepthOrArraySize, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
    auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(GetMemoryAllocator()->CreateResource(
        &heapDesc,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        &allocation,
        IID_PPV_ARGS(&resource)
    ));

    SetName(resource, "Intermediate upload");
}

}
