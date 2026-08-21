#include "pch.h"
#include "MeshPool.h"
#include "DeviceResources.h"
#include "ScopedTimer.h"
#include "Utility.h"

namespace neon::gfx {

constexpr uint64 CalculateMeshSize(const Mesh& mesh, uint64 alignment) {
    uint64 totalSize = 0;

    for (auto& submesh : mesh.submeshes) {
        totalSize += GetVectorSizeInBytes(submesh.vertices);
        totalSize = AlignTo(totalSize, alignment);

        totalSize += GetVectorSizeInBytes(submesh.opaqueIndices);
        totalSize = AlignTo(totalSize, alignment);

        totalSize += GetVectorSizeInBytes(submesh.additiveIndices);
        totalSize = AlignTo(totalSize, alignment);

        totalSize += GetVectorSizeInBytes(submesh.transparentIndices);
        totalSize = AlignTo(totalSize, alignment);

        //totalSize += GetVectorSizeInBytes(submesh.textures);
        //totalSize = AlignTo(totalSize, alignment);
    }

    return totalSize;
}

constexpr uint64 CalculateTextureIndexSize(const Mesh& mesh) {
    uint64 totalSize = 0;

    for (auto& submesh : mesh.submeshes) {
        totalSize += GetVectorSizeInBytes(submesh.textureHandles);
        totalSize = AlignTo(totalSize, 4);
    }

    return totalSize;
}

MeshID MeshPool::Upload(Mesh& mesh) {
    int64 time = 0;
    ScopedTimer timer(time);

    auto device = GetDevice();
    ASSERT(device);
    std::lock_guard lock(_uploadMutex);

    auto& resources = GetDeviceResources();

    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
    uploadContext.Reset();

    auto cmdList = uploadContext.GetCommandList();

    auto& uploadBuffer = resources.meshUploadBuffer;
    uploadBuffer.Clear();

    auto meshBufferSize = CalculateMeshSize(mesh, 4);
    if (meshBufferSize == 0) {
        SPDLOG_WARN("Tried to create an empty mesh!");
        return MeshID::None;
    }

    auto handle = (MeshID)_meshes.size();
    auto& gpuMesh = _meshes.emplace_back();
    gpuMesh.meshData.Create(mesh.name, meshBufferSize);
    gpuMesh.textureHandles.Create(mesh.name + " texture indices", CalculateTextureIndexSize(mesh));
    //gpuMesh.model = mesh.model;

    for (int j = 0; j < mesh.submeshes.size(); ++j) {
        auto& submesh = mesh.submeshes[j];
        //submesh.handle = handle;

        auto& gpuSubmesh = gpuMesh.submeshes.emplace_back();
        //gpuSubmesh.model = submesh.model;
        if (submesh.vertices.size() == 0 || submesh.opaqueIndices.size() == 0) continue;

        // Vertices
        {
            auto sizeInBytes = GetVectorSizeInBytes(submesh.vertices);

            //gpuSubmesh.vertexBuffer.Create(fmt::format("{} VB{:02}", mesh.name, i), sizeInBytes);
            auto srcOffset = uploadBuffer.CopyRange(span{ submesh.vertices });
            auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
            uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
            //uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.vertexBuffer, 0, srcOffset, sizeInBytes);

            auto& vbv = gpuSubmesh.vbv;
            vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
            // vbv.BufferLocation = gpuSubmesh.vertexBuffer->GetGPUVirtualAddress();
            vbv.SizeInBytes = (uint)sizeInBytes;
            vbv.StrideInBytes = sizeof(shaders::ModelVertex);
        }

        // Opaque indices
        {
            auto sizeInBytes = GetVectorSizeInBytes(submesh.opaqueIndices);

            // gpuSubmesh.indexBuffer.Create(fmt::format("{} IB{:02}", mesh.name, i), sizeInBytes);
            auto srcOffset = uploadBuffer.CopyRange(span{ submesh.opaqueIndices });
            auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
            uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
            // uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.indexBuffer, 0, srcOffset, sizeInBytes);

            auto& vbv = gpuSubmesh.opaqueIbv;
            vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
            // vbv.BufferLocation = gpuSubmesh.indexBuffer->GetGPUVirtualAddress();
            vbv.SizeInBytes = (uint)sizeInBytes;
            vbv.Format = DXGI_FORMAT_R16_UINT;
            gpuSubmesh.elementCount = (uint)submesh.opaqueIndices.size();
        }

        // Additive indices
        if (submesh.additiveIndices.size() > 0) {
            auto sizeInBytes = GetVectorSizeInBytes(submesh.additiveIndices);

            auto srcOffset = uploadBuffer.CopyRange(span{ submesh.additiveIndices });
            auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
            uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);

            auto& vbv = gpuSubmesh.additiveIbv;
            vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
            vbv.SizeInBytes = (uint)sizeInBytes;
            vbv.Format = DXGI_FORMAT_R16_UINT;

            gpuSubmesh.additiveElementCount = (uint)submesh.additiveIndices.size();
        }

        // Transparent indices
        if (submesh.transparentIndices.size() > 0) {
            auto sizeInBytes = GetVectorSizeInBytes(submesh.transparentIndices);

            auto srcOffset = uploadBuffer.CopyRange(span{ submesh.transparentIndices });
            auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
            uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);

            auto& vbv = gpuSubmesh.transparentIbv;
            vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
            vbv.SizeInBytes = (uint)sizeInBytes;
            vbv.Format = DXGI_FORMAT_R16_UINT;

            gpuSubmesh.transparentElementCount = (uint)submesh.transparentIndices.size();
        }

        // Texture handles
        {
            auto sizeInBytes = GetVectorSizeInBytes(submesh.textureHandles);

            // gpuMesh.textureMap.Create(fmt::format("{} TB{:02}", mesh.name, i), sizeInBytes);
            auto allocation = gpuMesh.textureHandles.Allocate(sizeInBytes);
            auto srcOffset = uploadBuffer.CopyRange(span{ submesh.textureHandles });
            uploadBuffer.CopyRegionTo(cmdList, gpuMesh.textureHandles, allocation.Offset, srcOffset, sizeInBytes);

            {
                auto& desc = gpuSubmesh.opaqueHandles;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                desc.Buffer.FirstElement = allocation.Offset / sizeof(int32);
                desc.Buffer.NumElements = (uint)submesh.opaqueIndices.size() / 3;
                desc.Buffer.StructureByteStride = sizeof(int32);
            }
            
            {
                auto& desc = gpuSubmesh.alphaHandles;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                desc.Buffer.FirstElement = allocation.Offset / sizeof(int32) + gpuSubmesh.opaqueHandles.Buffer.NumElements;
                desc.Buffer.NumElements = (uint)submesh.transparentIndices.size() / 3;
                desc.Buffer.StructureByteStride = sizeof(int32);
            }

            {
                auto& desc = gpuSubmesh.additiveHandles;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                desc.Buffer.FirstElement = allocation.Offset / sizeof(int32) + gpuSubmesh.opaqueHandles.Buffer.NumElements + gpuSubmesh.alphaHandles.Buffer.NumElements;
                desc.Buffer.NumElements = (uint)submesh.additiveIndices.size() / 3;
                desc.Buffer.StructureByteStride = sizeof(int32);
            }

            gpuSubmesh.texture = (TexID)submesh.textureHandles[0];
        }
    }


    uploadContext.Execute();
    uploadContext.WaitForIdle();

    //auto id = (MeshID)_meshes.size();
    //_meshes.push_back(std::move(mesh));

    timer.Stop();
    SPDLOG_INFO("Model upload time: {:.2f} ms", time / 1000.0f);
    return handle;
}

}
