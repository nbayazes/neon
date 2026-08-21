#include "pch.h"
#include "Mesh.h"
#include "d3/OutrageModel.h"
#include "d3/OutrageTable.h"
#include "DeviceResources.h"
#include "ModelCache.h"
#include "Utility.h"

namespace neon {

void PopulateTangents(span<gfx::shaders::ModelVertex> verts) {
    ASSERT(verts.size() == 3);
    auto edge1 = verts[1].position - verts[0].position;
    auto edge2 = verts[2].position - verts[0].position;
    auto deltaUV1 = verts[1].uv - verts[0].uv;
    auto deltaUV2 = verts[2].uv - verts[0].uv;

    static_assert(std::numeric_limits<float>::is_iec559); // Check that nan / inf behavior is defined
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    if (std::isnan(f) || std::isinf(f)) {
        // Invalid UVs or untextured side
        edge1.Normalize(verts[0].tangent);
        verts[1].tangent = verts[2].tangent = verts[0].tangent;
        auto bitangent = verts[0].tangent.Cross(verts[0].normal);
        verts[0].bitangent = verts[1].bitangent = verts[2].bitangent = bitangent;
    }
    else {
        Vector3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * f;
        tangent.Normalize();

        Vector3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * f;
        bitangent.Normalize();

        verts[0].tangent = verts[1].tangent = verts[2].tangent = tangent;
        verts[0].bitangent = verts[1].bitangent = verts[2].bitangent = bitangent;
    }
}

List<gfx::shaders::ModelVertex> MeshifySubmodel(d3::Model& model, d3::Submodel& submodel, span<d3::ModelFace> faces, List<int32>& textureHandles) {
    //gfx::Submesh submesh;
    //int16 index = 0;

    List<gfx::shaders::ModelVertex> vertices;

    for (auto& face : faces) {
        if (face.texNum == -1)
            continue; // Skip untextured faces as they are metadata such as gunpoints or glows

        Color color = face.color;

        const auto& fv0 = face.vertices[0];
        const auto& v0 = submodel.vertices[fv0.index];

        auto fvx = &face.vertices[1];
        auto vx = &submodel.vertices[fvx->index];

        // convert triangle fans to triangle lists
        for (int i = 2; i < face.vertices.size(); i++) {
            auto& fv = face.vertices[i];
            auto& v = submodel.vertices[fv.index];
            auto startSize = vertices.size();

            auto addVert = [&](const d3::Submodel::Vertex& vtx, const Vector2& uv) {
                color.A(vtx.alpha);

                vertices.push_back(gfx::shaders::ModelVertex{
                    .position = vtx.position,
                    .uv = uv,
                    .color = color,
                    .normal = vtx.normal,
                });
            };

            addVert(v0, fv0.uv);
            addVert(*vx, fvx->uv);
            addVert(v, fv.uv);

            textureHandles.push_back(model.textureHandles[face.texNum]);

            PopulateTangents(std::span{ &vertices[startSize], 3 });

            fvx = &fv;
            vx = &v;
        }
    }

    return vertices;
}

gfx::Mesh CreateMesh(d3::Model& model, span<d3::TextureFlag> flags) {
    ASSERT(model.textures.size() == flags.size());
    gfx::Mesh mesh;

    for (int smIndex = 0; auto& submodel : model.submodels) {
        auto& submesh = mesh.submeshes.emplace_back();
        //int16 index = 0;

        // split meshes into opaque, alpha, and additive so they can fit into render passes
        List<d3::ModelFace> opaque, alpha, additive;

        for (auto& face : submodel.faces) {
            if (face.texNum == -1)
                continue; // Skip untextured faces as they are metadata such as gunpoints or glows

            if (HasFlag(flags[face.texNum], d3::TextureFlag::Saturate)) {
                additive.push_back(face);
            }
            else if (HasFlag(flags[face.texNum], d3::TextureFlag::Alpha)) {
                alpha.push_back(face);
            }
            else {
                opaque.push_back(face);
            }
        }

        auto opaqueVertices = MeshifySubmodel(model, submodel, opaque, submesh.textureHandles);
        auto alphaVertices = MeshifySubmodel(model, submodel, alpha, submesh.textureHandles);
        auto additiveVertices = MeshifySubmodel(model, submodel, additive, submesh.textureHandles);

        // NOTE: texture indices will restart at zero for each submesh due to the ibv using an offset.
        //       the vertex buffer is shared between all

        // Combine the mesh data in the order: opaque, alpha, additive
        submesh.vertices = opaqueVertices;
        Seq::append(submesh.vertices, alphaVertices);
        Seq::append(submesh.vertices, additiveVertices);

        for (uint16 i = 0; i < opaqueVertices.size(); ++i)
            submesh.opaqueIndices.push_back(i);

        uint16 offset = (uint16)submesh.opaqueIndices.size();

        for (uint16 i = 0; i < alphaVertices.size(); ++i)
            submesh.transparentIndices.push_back(i + offset);

        offset += (uint16)submesh.transparentIndices.size();

        for (uint16 i = 0; i < additiveVertices.size(); ++i)
            submesh.additiveIndices.push_back(i + offset);

        smIndex++;
    }

    return mesh;
}

}
