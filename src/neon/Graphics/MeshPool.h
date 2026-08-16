#pragma once
#include "GraphicsHandles.h"
#include "Mesh.h"
#include "Utility.h"

namespace neon::gfx {


// Uploads meshes and stores their handles
class MeshPool {
    List<GpuMesh> _meshes;
    std::mutex _uploadMutex;
public:

    MeshID Upload(Mesh& mesh);

    GpuMesh* Get(MeshID id) {
        return Seq::tryItem(_meshes, (int)id);
    }
};

//inline MeshPool g_MeshPool;

}
