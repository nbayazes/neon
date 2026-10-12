#include "pch.h"
#include "VisualEffect.h"
#include "Graphics/Mesh.h"
#include "neon-math.h"

namespace neon::gfx {

Mesh GenerateShockwaveMesh(uint segments, uint textureRepeat) {
    Mesh mesh;
    auto& ringMesh = mesh.submeshes.emplace_back();
    ringMesh.vertices.resize(segments * 2);
    ringMesh.additiveIndices.resize(segments * 2);

    float angleIncrement = DirectX::XM_2PI / (segments - 1);
    float uIncrement = (float)textureRepeat / (float)(segments - 1);
    float angle = 0;
    float u = 0;

    for (uint16 i = 0; i < segments; i++, angle += angleIncrement, u += uIncrement) {
        auto point = Vector3(sin(angle), 0, cos(angle));

        auto& inner = ringMesh.vertices[i * 2];
        inner.position = point * 0.5f;
        inner.color.w = 0;
        inner.uv = Vector2(u, 0);

        auto& outer = ringMesh.vertices[i * 2 + 1];
        outer.position = point;
        outer.uv = Vector2(u, 1);

        //outerRing[i].position = Vector3(sin(angle), cos(angle), 0);
        //innerRing[i].position = outerRing[i].position * 0.5f;
        //innerRing[i].color.w = 0;
    }

    //for (int i = 0; i < segments; i++) {
    //    int next = (i + 1) % segments;
    //    outerRing[i].uv = Vector2(0, 0);
    //    outerRing[next].uv = Vector2(1, 0);
    //    innerRing[next].uv = Vector2(1, 1);
    //    innerRing[i].uv = Vector2(0, 1);
    //}

    for (uint16 i = 0; i < segments * 2u; i++) {
        ringMesh.additiveIndices[i] = i;
    }

    return mesh;
}

}
