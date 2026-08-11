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

        //for (uint slice = 0; slice < metadata.arraySize; ++slice) {
        //    for (uint mip = 0; mip < metadata.mipLevels; ++mip) {
        //        auto img = image.GetImage(mip, slice, 0);

        //        subresources[mip] = {
        //            .pData = img->pixels,
        //            .RowPitch = (LONG_PTR)img->rowPitch,
        //            .SlicePitch = (LONG_PTR)img->slicePitch
        //        };
        //    }
        //}

        auto count = image.GetImageCount();
        //std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        //for (uint slice = 0; slice < metadata.arraySize; ++slice) {
        //    subresources.clear();
        //    subresources.resize(metadata.mipLevels);

        //    for (uint mip = 0; mip < metadata.mipLevels; ++mip) {
        //        auto img = image.GetImage(mip, slice, 0);

        //        subresources[mip] = {
        //            .pData = img->pixels,
        //            .RowPitch = (LONG_PTR)img->rowPitch,
        //            .SlicePitch = (LONG_PTR)img->slicePitch
        //        };
        //    }

        //    UINT subresource = mip * metadata.arraySize + slice;
        //    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        //    UINT numRows; UINT64 rowSize, requiredSize;
        //    GetDevice()->GetCopyableFootprints(&_desc, subresource, 1, 0,
        //                                  &layout, &numRows, &rowSize, &requiredSize);

        //    UpdateSubresources(cmdList, _resource.Get(), intermediate.resource.Get(),
        //                       slice, 0, (uint)subresources.size(), subresources.data());
        //}

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
        GetDevice()->GetCopyableFootprints(&_desc, 0, subresources.size(), 0,
                                           layouts.data(), numRows.data(), rowSizes.data(), &requiredSize);

        UpdateSubresources(cmdList, _resource.Get(), intermediate.resource.Get(),
                           0, subresources.size(), requiredSize,
                           layouts.data(), numRows.data(), rowSizes.data(), subresources.data());

        //std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(metadata.arraySize);
        //std::vector<UINT>   numRows(metadata.arraySize);
        //std::vector<UINT64> rowSizes(metadata.arraySize);
        //UINT64 requiredSize = 0;
        //GetDevice()->GetCopyableFootprints(
        //    &_desc,          // the Texture2DArray
        //    0,                       // first subresource (mip0 slice0)
        //    metadata.arraySize,              // number of subresources to update
        //    0,                       // base offset in intermediate buffer
        //    layouts.data(), numRows.data(), rowSizes.data(), &requiredSize);
    }

    Transition(cmdList, D3D12_RESOURCE_STATE_COMMON);
    return intermediate;
}
}
