#pragma once

#include <DirectXTex.h>
#include <directx/d3dx12.h>
#include "neon-graphics.h"
#include "neon-types.h"

namespace neon::gfx {
    // General purpose image. Supports loading data from various formats.
    class Image : public DirectX::ScratchImage {
    public:
        Image Clone() const {
            Image image;
            if (SUCCEEDED(image.Initialize(GetMetadata())))
                memcpy(image.GetPixels(), GetPixels(), GetPixelsSize());

            return image;
        }

        bool Empty() const {
            auto& metadata = GetMetadata();
            return metadata.width == 0 || metadata.height == 0;
        }

        bool GetPitch(size_t& rowPitch, size_t& slicePitch) const {
            auto& metadata = GetMetadata();
            return SUCCEEDED(DirectX::ComputePitch(metadata.format, metadata.width, metadata.height, rowPitch, slicePitch));
        }

        D3D12_RESOURCE_DESC GetResourceDesc() const {
            auto& md = GetMetadata();

            if (md.IsVolumemap()) {
                // 3D texture
                return CD3DX12_RESOURCE_DESC::Tex3D(md.format, md.width, (uint)md.height, (uint16)md.depth, (uint16)md.mipLevels);
            }
            else if (md.height == 1) {
                // 1D texture
                return CD3DX12_RESOURCE_DESC::Tex1D(md.format, md.width, (uint16)md.arraySize, (uint16)md.mipLevels);
            }
            else {
                // 2D texture or cubemap
                return CD3DX12_RESOURCE_DESC::Tex2D(md.format, md.width, (uint)md.height, (uint16)md.arraySize, (uint16)md.mipLevels);
            }
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC GetShaderViewDesc() const {
            auto& metadata = GetMetadata();

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{
                .Format = metadata.format,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            };

            if (metadata.IsCubemap()) {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                desc.TextureCube.MipLevels = (uint)metadata.mipLevels;
                return desc;
            }
            else if (metadata.IsVolumemap()) {
                // 3D texture
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                desc.Texture3D.MipLevels = (uint)metadata.mipLevels;
                return desc;
            }
            else {
                // 2D texture
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipLevels = (uint)metadata.mipLevels;
                return desc;
            }
        }

        bool HasMipmaps() const {
            return GetMetadata().mipLevels > 1;
        }

        bool GenerateMipmaps(bool wrapu = true, bool wrapv = true) {
            if (Empty()) return false;

            using namespace DirectX;
            auto flags = TEX_FILTER_DEFAULT;
            if (wrapu) flags |= TEX_FILTER_WRAP_U;
            if (wrapv) flags |= TEX_FILTER_WRAP_V;

            ScratchImage source;
            if (FAILED(source.InitializeFromImage(*GetImage(0, 0, 0))))
                return false;

            auto srcImage = source.GetImage(0, 0, 0);
            size_t levels = 0; // 0 generates all levels
            return SUCCEEDED(DirectX::GenerateMipMaps(*srcImage, flags, levels, *this));
        }


        bool Resize(bool wrapu, bool wrapv, uint8 width, uint8 height) {
            using namespace DirectX;
            auto flags = TEX_FILTER_DEFAULT;
            if (wrapu) flags |= TEX_FILTER_WRAP_U;
            if (wrapv) flags |= TEX_FILTER_WRAP_V;

            ScratchImage source;
            if (FAILED(source.InitializeFromImage(*GetImage(0, 0, 0))))
                return false;

            auto srcImage = source.GetImage(0, 0, 0);

            // replace the buffer
            return SUCCEEDED(DirectX::Resize(*srcImage, width, height, flags, *this));
        }

        // loads raw data into an image
        template <class TData>
        bool Load(span<const TData> data, size_t width, size_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            size_t rowPitch, slicePitch;
            if (FAILED(DirectX::ComputePitch(format, width, height, rowPitch, slicePitch)))
                return false;

            DirectX::Image image(width, height, format, rowPitch, slicePitch, (uint8*)data.data());
            return SUCCEEDED(InitializeFromImage(image));
        }

        template <class TData>
        bool LoadMipmapped(span<std::vector<TData>> mipData, size_t width, size_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            if (FAILED(Initialize2D(format, width, height, 1, mipData.size())))
                return false;

            // Fill each level
            for (size_t mip = 0; mip < mipData.size(); ++mip) {
                const auto img = GetImage(mip, 0, 0);
                memcpy(img->pixels, mipData[mip].data(), img->slicePitch);
            }

            return true;
        }

        // SRGB indicates whether to treat the source image as SRGB or linear
        // This assumes the source is using straight alpha (not premultiplied)
        bool LoadWIC(span<ubyte> source, bool srgb) {
            using namespace DirectX;
            auto flags = srgb ? WIC_FLAGS_DEFAULT_SRGB : WIC_FLAGS_FORCE_LINEAR;

            ScratchImage result, premultiplied;
            if (FAILED(LoadFromWICMemory(source.data(), source.size(), flags, nullptr, result)))
                return false;

            auto image = result.GetImage(0, 0, 0);
            return SUCCEEDED(PremultiplyAlpha(*image, TEX_PMALPHA_DEFAULT, *this));
        }

        // This assumes the source is using straight alpha (not premultiplied)
        bool LoadTGA(span<ubyte> source, bool srgb) {
            using namespace DirectX;

            auto flags = srgb ? TGA_FLAGS_DEFAULT_SRGB : TGA_FLAGS_FORCE_LINEAR;

            ScratchImage result, premultiplied;
            if (FAILED(LoadFromTGAMemory(source.data(), source.size(), flags, nullptr, result)))
                return false;

            auto image = result.GetImage(0, 0, 0);
            return SUCCEEDED(PremultiplyAlpha(*image, TEX_PMALPHA_DEFAULT, *this));
        }

        bool LoadDDS(span<ubyte> source, bool srgb) {
            using namespace DirectX;
            ScratchImage dds, decompressed;

            if (FAILED(LoadFromDDSMemory(source.data(), source.size(), DDS_FLAGS_NONE, nullptr, *this)))
                return false;

            auto& metadata = GetMetadata();
            if (srgb) OverrideFormat(MakeSRGB(metadata.format));
            return true;
        }
    };

    Option<Image> ReadImage(string_view name, bool srgb);
}
