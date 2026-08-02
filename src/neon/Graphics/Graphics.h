#pragma once
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

void ScreenSizeChanged(unsigned int width, unsigned int height);

//void CreateDevice(DeviceCreationOptions& options);
void Shutdown();

// Waits for the GPU to become idle
void WaitForGpu();

void RenderView(Camera& camera);

// Presents to the screen
void Present();

using TexHandle = unsigned int;

// Returns the handle of the texture. Used later to free or retrieve it
// todo: if name alreadly exists, replace it and swap
TexHandle CreateTexture(const Image& image, std::string_view name);

Texture* GetTexture(TexHandle index);

void FreeTexture(TexHandle index);

using GpuMeshHandle = int;

struct Submesh {
    List<gfx::shaders::ModelVertex> vertices;
    List<uint16> indices;
    List<short> textures; // local texture index for each triangle
    GpuMeshHandle handle = -1; // Handle to the GPU mesh data. Used for retrieval and updating

    //D3D12_VERTEX_BUFFER_VIEW vbv{};
    //D3D12_INDEX_BUFFER_VIEW ibv{};
};

struct Mesh {
    string name;
    List<Submesh> submeshes;
    bool IsTransparent = false; // contains transparent textures
};

GpuMeshHandle CreateMesh();

void UploadMeshes(span<Mesh> meshes);
// todo: must have a way to reassign textures without stalling
void UpdateMeshTextures(Mesh& mesh);


ID3D12Device* GetDevice();

}
