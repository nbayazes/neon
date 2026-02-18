#pragma once
#include "Texture.h"

namespace neon::gfx {
    class Image;

    struct DeviceCreationOptions {
        bool enableDebugging = false;
        bool allowTearing = false; // VRR support
        bool enableHdr = false;
        bool useVsync = false; // overrides VRR support
    };

    void Init(HWND hwnd, unsigned int width, unsigned int height, DeviceCreationOptions& options);

    void ScreenSizeChanged(unsigned int width, unsigned int height);

    //void CreateDevice(DeviceCreationOptions& options);
    void Shutdown();

    // Waits for the GPU to become idle
    void WaitForGpu();

    // Presents to the screen
    void Present();

    unsigned int CreateTexture(const Image& image, std::string_view name);

    Texture* GetTexture(unsigned int index);

    Texture* GetWhiteTexture();

    void FreeTexture(unsigned int index);
}
