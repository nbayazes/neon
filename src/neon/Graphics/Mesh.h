#pragma once
#include "GpuResource.h"
#include "GraphicsHandles.h"
#include "neon-types.h"
#include "shaders/ModelVertex.h"

namespace neon::gfx {

struct GpuSubmesh {
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW opaqueIbv{};
    D3D12_INDEX_BUFFER_VIEW transparentIbv{};
    D3D12_INDEX_BUFFER_VIEW additiveIbv{};
    D3D12_SHADER_RESOURCE_VIEW_DESC opaqueHandles{};
    D3D12_SHADER_RESOURCE_VIEW_DESC alphaHandles{};
    D3D12_SHADER_RESOURCE_VIEW_DESC additiveHandles{};
    uint elementCount = 0;
    uint transparentElementCount = 0;
    uint additiveElementCount = 0;
    TexID texture = TexID::None; // for sprites
};

// GPU instanced mesh
struct GpuMesh {
    gfx::GpuBuffer meshData;
    gfx::GpuBuffer textureHandles;
    List<GpuSubmesh> submeshes;
};

// Submesh upload
struct Submesh {
    List<gfx::shaders::ModelVertex> vertices;
    List<uint16> opaqueIndices;
    List<uint16> transparentIndices;
    List<uint16> additiveIndices;
    List<int32> textureHandles; // local texture index for each triangle
};

// Mesh upload
struct Mesh {
    string name;
    List<Submesh> submeshes;
    bool IsTransparent = false; // contains transparent textures
};

//gfx::Mesh CreateMesh(d3::Model& model);


//void UploadMeshes(span<Mesh> meshes);

//void UploadModel(ModelID id);

// todo: must have a way to reassign textures without stalling
//void UpdateMeshTextures(Mesh& mesh);

}
