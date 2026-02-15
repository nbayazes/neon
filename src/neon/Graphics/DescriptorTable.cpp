#include "pch.h"
#include "DescriptorTable.h"
//#include "DeviceResources.h"

namespace neon::gfx {
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AddRTV(GpuResource& resource, uint index) const {
        ASSERT(_desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        resource._rtv = GetHandle(index);
        auto desc = resource.GetRtvDesc();
        _device->CreateRenderTargetView(resource.Get(), &desc, resource._rtv.GetCpuHandle());
        return resource.GetRTV();
    }
}
