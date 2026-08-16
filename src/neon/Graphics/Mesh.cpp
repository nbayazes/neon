#include "pch.h"
#include "Mesh.h"
#include "d3/OutrageModel.h"
#include "DeviceResources.h"
#include "ModelCache.h"
#include "ScopedTimer.h"
#include "Utility.h"

namespace neon {

namespace {
    std::mutex _uploadMutex;
}


void PopulateTangents(span<gfx::shaders::ModelVertex> verts) {
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

gfx::Mesh CreateMesh(d3::Model& model) {
    //auto model = g_ModelCache.Get(handle);
    //if (!model) return {};

    gfx::Mesh mesh;
    //mesh.model = handle;

    for (int smIndex = 0; auto& submodel : model.submodels) {
        auto& submesh = mesh.submeshes.emplace_back();
        int16 index = 0;

        // combine uvs from faces with the vertices
        for (auto& face : submodel.faces) {
            if (face.texNum == -1) continue; // Skip untextured faces as they are metadata such as gunpoints or glows
            // todo: split meshes based on transparency - were the original models designed with this in mind?
            Color color = face.color;

            const auto& fv0 = face.vertices[0];
            const auto& v0 = submodel.vertices[fv0.index];

            auto fvx = &face.vertices[1];
            auto vx = &submodel.vertices[fvx->index];

            // convert triangle fans to triangle lists
            for (int i = 2; i < face.vertices.size(); i++) {
                auto& fv = face.vertices[i];
                auto& v = submodel.vertices[fv.index];
                auto startSize = submesh.vertices.size();

                auto addVert = [&](const d3::Submodel::Vertex& vtx, const Vector2& uv) {
                    color.A(vtx.alpha);

                    submesh.vertices.push_back(gfx::shaders::ModelVertex{
                        .position = vtx.position,
                        .uv = uv,
                        .color = color,
                        .normal = vtx.normal,
                                               });
                    submesh.indices.push_back(index++);
                };

                addVert(v0, fv0.uv);
                addVert(*vx, fvx->uv);
                addVert(v, fv.uv);

                PopulateTangents(std::span{ &submesh.vertices[startSize], 3 });

                fvx = &fv;
                vx = &v;

                // Map the local indices to global ones
                submesh.textureHandles.push_back(model.textureHandles[face.texNum]);
                //submesh.model = submodel;
            }
        }

        smIndex++;
    }

    return mesh;
}

//constexpr uint64 CalculateMeshSize(const gfx::Mesh& mesh, uint64 alignment) {
//    uint64 totalSize = 0;
//
//    for (auto& submesh : mesh.submeshes) {
//        totalSize += GetVectorSizeInBytes(submesh.vertices);
//        totalSize = AlignTo(totalSize, alignment);
//
//        totalSize += GetVectorSizeInBytes(submesh.indices);
//        totalSize = AlignTo(totalSize, alignment);
//
//        //totalSize += GetVectorSizeInBytes(submesh.textures);
//        //totalSize = AlignTo(totalSize, alignment);
//    }
//
//    return totalSize;
//}
//
//constexpr uint64 CalculateTextureIndexSize(const gfx::Mesh& mesh) {
//    uint64 totalSize = 0;
//
//    for (auto& submesh : mesh.submeshes) {
//        totalSize += GetVectorSizeInBytes(submesh.textureIndices);
//        totalSize = AlignTo(totalSize, 4);
//    }
//
//    return totalSize;
//}


// Uploads multiple meshes and fills in their GPU views
//void UploadMeshes(span<Mesh> meshes) {
//    int64 time = 0;
//    ScopedTimer timer(time);
//
//    auto device = GetDevice();
//    ASSERT(device);
//    std::lock_guard lock(_uploadMutex);
//
//    auto& resources = GetDeviceResources();
//
//    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
//    uploadContext.Reset();
//
//    auto cmdList = uploadContext.GetCommandList();
//
//    auto& uploadBuffer = resources.meshUploadBuffer;
//    uploadBuffer.Clear();
//
//    // All buffers must use CBV alignment if they are packed in a single shared buffer
//
//    for (int i = 0; i < meshes.size(); ++i) {
//        auto& mesh = meshes[i];
//        auto meshBufferSize = CalculateMeshSize(mesh, 4);
//
//        auto& gpuMesh = resources.meshes.emplace_back();
//        gpuMesh.meshData.Create(mesh.name, meshBufferSize);
//        gpuMesh.textureIndices.Create(mesh.name + " texture indices", CalculateTextureIndexSize(mesh));
//        //gpuMesh.model = mesh.model;
//
//        for (int j = 0; j < mesh.submeshes.size(); ++j) {
//            auto& submesh = mesh.submeshes[j];
//            if (submesh.vertices.size() == 0 || submesh.indices.size() == 0) continue;
//            submesh.handle = (int)resources.meshes.size();
//
//            auto& gpuSubmesh = gpuMesh.submeshes.emplace_back();
//            //gpuSubmesh.model = submesh.model;
//
//            {
//                auto sizeInBytes = GetVectorSizeInBytes(submesh.vertices);
//
//                //gpuSubmesh.vertexBuffer.Create(fmt::format("{} VB{:02}", mesh.name, i), sizeInBytes);
//                auto srcOffset = uploadBuffer.Copy(span{ submesh.vertices });
//                auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
//                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
//                //uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.vertexBuffer, 0, srcOffset, sizeInBytes);
//
//                auto& vbv = gpuSubmesh.vbv;
//                vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
//                // vbv.BufferLocation = gpuSubmesh.vertexBuffer->GetGPUVirtualAddress();
//                vbv.SizeInBytes = (uint)sizeInBytes;
//                vbv.StrideInBytes = sizeof(shaders::ModelVertex);
//
//                gpuSubmesh.elementCount = (uint)submesh.vertices.size();
//            }
//
//            {
//                auto sizeInBytes = GetVectorSizeInBytes(submesh.indices);
//
//                // gpuSubmesh.indexBuffer.Create(fmt::format("{} IB{:02}", mesh.name, i), sizeInBytes);
//                auto srcOffset = uploadBuffer.Copy(span{ submesh.indices });
//                auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
//                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
//                // uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.indexBuffer, 0, srcOffset, sizeInBytes);
//
//                auto& vbv = gpuSubmesh.ibv;
//                vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
//                // vbv.BufferLocation = gpuSubmesh.indexBuffer->GetGPUVirtualAddress();
//                vbv.SizeInBytes = (uint)sizeInBytes;
//                vbv.Format = DXGI_FORMAT_R16_UINT;
//            }
//
//            {
//                auto sizeInBytes = GetVectorSizeInBytes(submesh.textureIndices);
//
//                // gpuMesh.textureMap.Create(fmt::format("{} TB{:02}", mesh.name, i), sizeInBytes);
//                auto allocation = gpuMesh.textureIndices.Allocate(sizeInBytes);
//                auto srcOffset = uploadBuffer.Copy(span{ submesh.textureIndices });
//                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.textureIndices, allocation.Offset, srcOffset, sizeInBytes);
//
//                auto& desc = gpuSubmesh.textureIndicesView;
//                desc.Format = DXGI_FORMAT_UNKNOWN;
//                desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//                desc.Buffer.FirstElement = allocation.Offset / sizeof(int32);
//                desc.Buffer.NumElements = (uint)submesh.textureIndices.size();
//                desc.Buffer.StructureByteStride = sizeof(int32);
//            }
//        }
//    }
//
//    uploadContext.Execute();
//    uploadContext.WaitForIdle();
//
//    timer.Stop();
//    SPDLOG_INFO("Model upload time: {:.2f} ms", time / 1000.0f);
//}

//void UploadModel(ModelID id) {
//    int64 time = 0;
//    ScopedTimer timer(time);
//
//    auto model = g_ModelCache.Get(id);
//    if(!model) return;
//
//    timer.Start();
//    LoadTextures(hog, gameTable, model->textures);
//    timer.Stop();
//    SPDLOG_INFO("Model texture load time: {:.2f} ms", time / 1000.0f);
//
//    timer.Start();
//    MapTextures(gameTable, model);
//    timer.Stop();
//    SPDLOG_INFO("Texture map time: {:.2f} ms", time / 1000.0f);
//
//    auto mesh = CreateMesh(model);
//    timer.Stop();
//    SPDLOG_INFO("Mesh load time: {:.2f} ms", time / 1000.0f);
//
//    gfx::UploadMeshes(upload);
//}

}
