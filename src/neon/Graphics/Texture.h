#pragma once
#include "neon-graphics.h"
#include "GpuResource.h"
#include "Image.h"

namespace neon::gfx {

class Texture final : public PixelBuffer {
    D3D12_SHADER_RESOURCE_VIEW_DESC _srvDesc{};

public:
    // Creates a texture from an image. Returns the intermediate resource used to upload the texture.
    [[nodiscard]] IntermediateResource Create(ID3D12GraphicsCommandList* cmdList, const Image& image, string_view name);

protected:
    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
        return _srvDesc;
    }
};

}
