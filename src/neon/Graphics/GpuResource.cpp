#include "pch.h"
#include "GpuResource.h"
#include "DeviceResources.h"
#include "neon.h"

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
        D3D12_RESOURCE_STATE_COMMON,
        clearValue,
        _allocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
    ));

    SetName(_resource, name);
}

void GpuBuffer::Create(string_view name, uint32 elementSize, uint32 elementCount) {
    _desc = CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount);
    _state = D3D12_RESOURCE_STATE_GENERIC_READ;

    //_srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    //_srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    //_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    //_srvDesc.Buffer.NumElements = elementCount;
    //_srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12MA::ALLOCATION_DESC allocDesc = { .HeapType = D3D12_HEAP_TYPE_DEFAULT };
    auto allocator = GetMemoryAllocator();

    ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &_desc,
        _state,
        nullptr,
        _allocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
    ));

    //if (!_srv) _srv = Render::Descriptors->reserved.Allocate();
    //Render::Device->CreateShaderResourceView(Get(), &_srvDesc, _srv.GetCpuHandle());
    SetName(_resource, name);
}

void GpuUploadBuffer::Create(string_view name, uint64 size) {
    _desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    _state = D3D12_RESOURCE_STATE_GENERIC_READ;

    auto allocator = GetMemoryAllocator();
    D3D12MA::ALLOCATION_DESC allocDesc = { .HeapType = D3D12_HEAP_TYPE_UPLOAD };

    ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &_desc,
        _state,
        nullptr,
        _allocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
    ));

    SetName(_resource, name);
}

IntermediateResource::IntermediateResource(const D3D12_RESOURCE_DESC& desc) {
    D3D12MA::ALLOCATION_DESC heapDesc{ .HeapType = D3D12_HEAP_TYPE_UPLOAD };

    UINT64 uploadBufferSize;
    neon::gfx::GetDevice()->GetCopyableFootprints(&desc, 0, desc.MipLevels, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
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
