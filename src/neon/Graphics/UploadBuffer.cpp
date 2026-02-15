#include "pch.h"
#include "UploadBuffer.h"

namespace neon::gfx {
    void CreateUploadHeap(ComPtr<ID3D12Resource>& resource, uint64 bufferSize) {
        auto props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        ThrowIfFailed(GetDevice()->CreateCommittedResource(
            &props,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
        ));
    }
}
