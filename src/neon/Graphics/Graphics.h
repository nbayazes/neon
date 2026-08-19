#pragma once
#include "Handles.h"
#include "shaders/Model.h"
#include "shaders/ModelVertex.h"
#include "Texture.h"

namespace neon {
class Camera;
}

namespace neon::gfx {

class Image;

struct DeviceCreationOptions {
    bool enableDebugging = false;
    bool allowTearing = false; // VRR support
    bool enableHdr = false;
    bool useVsync = false; // overrides VRR support
};

void Init(HWND hwnd, unsigned int width, unsigned int height, DeviceCreationOptions& options);

void ReloadShaders();

void ScreenSizeChanged(unsigned int width, unsigned int height);

//void CreateDevice(DeviceCreationOptions& options);
void Shutdown();

// Waits for the GPU to become idle
void WaitForGpu();

void RenderView(Camera& camera, ModelID modelid);

// Presents to the screen
void Present();

ID3D12Device* GetDevice();

void UpdateTextureInfo(const span<TextureInfo>& textures);

}
