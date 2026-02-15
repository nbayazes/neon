#include "pch.h"
#include "GpuResource.h"
#include "DeviceResources.h"
#include "neon.h"

namespace neon::gfx {
    void GpuResource::Create(D3D12_HEAP_TYPE heapType, string_view name, const D3D12_CLEAR_VALUE* clearValue, bool forceComitted) {
        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = heapType;
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
