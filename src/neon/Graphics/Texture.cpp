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
        // Copy all mips and slices
        std::vector<D3D12_SUBRESOURCE_DATA> subresources(metadata.mipLevels * metadata.arraySize);

        for (uint mip = 0; mip < metadata.mipLevels; ++mip) {
            for (uint slice = 0; slice < metadata.arraySize; ++slice) {
                auto img = image.GetImage(mip, slice, 0);
                subresources[slice * metadata.mipLevels + mip] = {
                    .pData = img->pixels,
                    .RowPitch = (LONG_PTR)img->rowPitch,
                    .SlicePitch = (LONG_PTR)img->slicePitch
                };
            }
        }

        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresources.size());
        std::vector<UINT> numRows(subresources.size());
        std::vector<UINT64> rowSizes(subresources.size());

        UINT64 requiredSize;
        GetDevice()->GetCopyableFootprints(&_desc, 0, (UINT)subresources.size(), 0,
                                           layouts.data(), numRows.data(), rowSizes.data(), &requiredSize);

        UpdateSubresources(cmdList, _resource.Get(), intermediate.resource.Get(),
                           0, (UINT)subresources.size(), requiredSize,
                           layouts.data(), numRows.data(), rowSizes.data(), subresources.data());
    }

    Transition(cmdList, D3D12_RESOURCE_STATE_COMMON);
    return intermediate;
}

TexID UploadTexture(const Image& image, std::string_view name, bool reserved) {
    auto& resources = GetDeviceResources();

    //auto index = reserved ? resources.reservedTextures.size() : resources.textures.size();
    auto& texture = reserved ? resources.reservedTextures.emplace_back() : resources.textures.emplace_back();
    resources.textureCopyContext->Reset();
    auto intermediate = texture.Create(resources.textureCopyContext->GetCommandList(), image, name);
    resources.textureCopyContext->Execute();
    resources.textureCopyContext->WaitForIdle();

    if (reserved) {
        resources.reservedDescriptors->AddSRV(texture).ptr;
        return (TexID)(resources.reservedDescriptors->Count() - 1);
    }
    else {
        resources.textureDescriptors->AddSRV(texture).ptr;
        return (TexID)(resources.textureDescriptors->Count() - 1);
    }

    //return (TexID)index;
}

Texture* GetTexture(TexID index, bool reserved) {
    auto& resources = GetDeviceResources();

    if (reserved) {
        if ((int)index >= resources.reservedTextures.size()) return nullptr;
        return &resources.reservedTextures[(int)index];
    } else {
        if ((int)index >= resources.textures.size()) return nullptr;
        return &resources.textures[(int)index];
    }
}

void FreeTexture(TexID index) {
    auto& resources = GetDeviceResources();
    // todo: this should queue the resource to be freed
    if ((int)index >= resources.textures.size()) return;
    resources.textures[(int)index] = {};
}

}
