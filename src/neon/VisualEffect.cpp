#include "pch.h"
#include "VisualEffect.h"
#include "Graphics/Mesh.h"
#include "neon-math.h"

namespace neon::gfx {

Mesh GenerateShockwaveMesh(uint16 segments, uint16 textureRepeat, float thickness) {
    Mesh mesh;
    auto& ringMesh = mesh.submeshes.emplace_back();
    ringMesh.vertices.reserve(segments * 2 * 4);
    ringMesh.additiveIndices.reserve(segments * 2 * 4 + 3);
    auto& vertices = ringMesh.vertices;

    float angleIncrement = DirectX::XM_2PI / (segments - 1);
    float uIncrement = (float)textureRepeat / (float)(segments - 1);
    float angle = 0;
    float u = 0;

    constexpr float midOffset = 0.55f;
    constexpr float midV = (midOffset - 0.5f) * 2;

    Array<List<uint16>, 4> ringIndices = {};
    for (auto& indices : ringIndices) {
        indices.reserve(segments * 2);
    }

    for (uint16 i = 0; i < segments; i++, angle += angleIncrement, u += uIncrement) {
        auto point = Vector3(sin(angle), 0, cos(angle));

        auto startIndex = (uint16)vertices.size();
        
        auto& inner = vertices.emplace_back();
        inner.position = point * 0.5f;
        inner.color.w = 0;
        inner.uv = Vector2(u, 0);

        auto& top = vertices.emplace_back();
        top.position = point * midOffset;
        top.position.y += thickness;
        top.uv = Vector2(u, midV);

        auto& bottom = vertices.emplace_back();
        bottom.position = point * midOffset;
        bottom.position.y -= thickness;
        bottom.uv = Vector2(u, midV);

        auto& outer = vertices.emplace_back();
        outer.position = point;
        outer.uv = Vector2(u, 1);

        // top surface
        ringIndices[0].push_back(startIndex + 0);
        ringIndices[0].push_back(startIndex + 1);

        ringIndices[1].push_back(startIndex + 1);
        ringIndices[1].push_back(startIndex + 3);

        // bottom surface
        ringIndices[2].push_back(startIndex + 3);
        ringIndices[2].push_back(startIndex + 2);

        ringIndices[3].push_back(startIndex + 2);
        ringIndices[3].push_back(startIndex + 0);
    }

    Seq::append(ringMesh.additiveIndices, ringIndices[0]);
    ringMesh.additiveIndices.push_back(D3D12_16BIT_INDEX_STRIP_CUT_VALUE);
    Seq::append(ringMesh.additiveIndices, ringIndices[1]);
    ringMesh.additiveIndices.push_back(D3D12_16BIT_INDEX_STRIP_CUT_VALUE);
    Seq::append(ringMesh.additiveIndices, ringIndices[2]);
    ringMesh.additiveIndices.push_back(D3D12_16BIT_INDEX_STRIP_CUT_VALUE);
    Seq::append(ringMesh.additiveIndices, ringIndices[3]);

    return mesh;
}

}
