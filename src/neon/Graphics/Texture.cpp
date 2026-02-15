#include "pch.h"
#include "Texture.h"
#include "DeviceResources.h"

namespace neon::gfx {
    IntermediateResource Texture::Create(ID3D12GraphicsCommandList* cmdList, const Image& image, string_view name) {
        if (image.Empty()) {
            __debugbreak(); // why upload an empty image?
            return {};
        }

        auto& metadata = image.GetMetadata();
        _srvDesc = image.GetShaderViewDesc();
        _desc = image.GetResourceDesc();

        auto intermediate = IntermediateResource(_desc);
        CreateOnDefaultHeap(name);

        Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

        {
            // copy mipmaps
            std::vector<D3D12_SUBRESOURCE_DATA> subresources(metadata.mipLevels);

            for (uint mip = 0; mip < metadata.mipLevels; ++mip) {
                auto img = image.GetImage(mip, 0, 0);

                subresources[mip] = {
                    .pData = img->pixels,
                    .RowPitch = (LONG_PTR)img->rowPitch,
                    .SlicePitch = (LONG_PTR)img->slicePitch
                };
            }

            UpdateSubresources(cmdList, _resource.Get(), intermediate.resource.Get(), 0, 0, (uint)subresources.size(), subresources.data());
        }

        Transition(cmdList, D3D12_RESOURCE_STATE_COMMON);
        return intermediate;
    }
}
