#pragma once
#include <d3d12.h>
#include "GraphicsHandles.h"
#include "Handles.h"
#include "neon-math.h"
#include "neon-types.h"

namespace neon::gfx {

enum class RenderPass { Opaque, Transparent, Additive, Count };
enum class DrawCommandType { Mesh, Sprite };

struct DrawCommand {
    DrawCommandType type;
    float depth = 0;
    //MeshID model = MeshID::None;

    uint count = 0; // element count for meshes, instance count for sprites
    D3D12_GPU_DESCRIPTOR_HANDLE descriptorTable; // starting descriptor
    D3D12_VERTEX_BUFFER_VIEW vertexBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBuffer;
};

class RenderQueue {
    List<DrawCommand> _queues[(int)RenderPass::Count];
public:
    void Add(const DrawCommand& command, RenderPass pass) {
        _queues[(int)pass].push_back(command);
    }

    void Clear() {
        for (int i = 0; i < (int)RenderPass::Count; ++i) {
            _queues[i].clear();
        }
    }

    span<DrawCommand> GetQueue(RenderPass pass) {
        return _queues[(int)pass];
    }
};

}
