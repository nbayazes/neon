#include "pch.h"
#include "Mesh.h"
#include "d3/OutrageModel.h"
#include "d3/OutrageTable.h"
#include "Device.h"
#include "Graphics.h"
#include "ModelCache.h"
#include "shaders/ModelPrepass.h"
#include "SpriteBatch.h"
#include "SystemClock.h"
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

List<gfx::shaders::ModelVertex> MeshifySubmodel(const d3::Model& model, d3::Submodel& submodel, span<d3::ModelFace> faces, List<int32>& textureHandles) {
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

namespace neon::gfx {

namespace {
    AnimationInstance _animation;

    // Accumulated transforms for a model
    List<Matrix> _modelTransforms;
}

// For a given animation frame find the relevant keyframes and interpolate between them
Quaternion InterpolateRotation(const d3::Submodel& submodel, float frame) {
    if (submodel.keyframes.empty())
        return Quaternion::Identity;

    // Assume that keyframes are sorted and exit early if frame is out of range
    if (frame >= submodel.keyframes.back().frame)
        return submodel.keyframes.back().rotation;

    if (frame <= submodel.keyframes.front().frame)
        return submodel.keyframes.front().rotation;

    for (int i = 1; i < submodel.keyframes.size(); ++i) {
        auto& next = submodel.keyframes[i];
        if (next.frame > (int)frame) {
            // found the next keyframe, get the previous keyframe
            auto& prev = submodel.keyframes[i - 1];
            float progress = frame - prev.frame;
            float range = float(next.frame - prev.frame);
            float alpha = progress / range;
            //SPDLOG_INFO("rotation frame: {} - {} alpha: {}", i - 1, i, alpha);
            return Quaternion::Slerp(prev.rotation, next.rotation, alpha);
        }
    }

    return Quaternion::Identity;
}

// For a given animation frame find the relevant keyframes and interpolate between them
Vector3 InterpolatePosition(const d3::Submodel& submodel, float frame) {
    if (submodel.positionKeyframes.empty())
        return Vector3::Zero;

    // Assume that keyframes are sorted and exit early if frame is out of range
    if (frame > submodel.positionKeyframes.back().frame)
        return submodel.positionKeyframes.back().position;

    if (frame < submodel.positionKeyframes.front().frame)
        return submodel.positionKeyframes.front().position;

    for (int i = 1; i < submodel.positionKeyframes.size(); ++i) {
        auto& next = submodel.positionKeyframes[i];
        if (next.frame > (int)frame) {
            // found the next keyframe, get the previous keyframe
            auto& prev = submodel.positionKeyframes[i - 1];
            float progress = frame - prev.frame;
            float range = float(next.frame - prev.frame);
            float alpha = progress / range;
            //SPDLOG_INFO("position frame: {} - {} alpha: {}", prev.frame, next.frame, alpha);
            return Vector3::Lerp(prev.position, next.position, alpha);
        }
    }

    return Vector3::Zero;
}

void AnimateModel(const d3::Model& model, AnimationInstance& animation, float dt) {
    _modelTransforms.resize(model.submodels.size());

    // todo: only update animations when animation is playing - this is complicated by constantly rotating submodels which rely on translations
    // consider caching rotations and translations separately per submodel
    float frame = animation.to * animation.timeScale;

    if (animation.elapsed < animation.duration) {
        float percent = animation.elapsed / animation.duration;
        float range = animation.to * animation.timeScale - animation.from * animation.timeScale;
        frame = range * percent + animation.from * animation.timeScale;
    }

    for (int sm = 0; sm < model.submodels.size(); ++sm) {
        auto& submodel = model.submodels[sm];

        Quaternion rotation = Quaternion::Identity;

        if (HasFlag(submodel.flags, d3::SubmodelFlag::Rotate)) {
            // original code uses keyframe 1 for the rotation axis
            if (submodel.keyframes.size() > 1)
                rotation = Quaternion::CreateFromAxisAngle(submodel.keyframes[1].axis, submodel.rotation * (float)Clock.GetTotalTimeSeconds());
        }
        else {
            rotation = InterpolateRotation(submodel, frame);
        }

        Vector3 position = InterpolatePosition(submodel, frame);

        auto translation = Matrix::CreateTranslation(submodel.offset + position);
        
        if (submodel.parent >= 0)
            _modelTransforms[sm] = Matrix::CreateFromQuaternion(rotation) * translation * _modelTransforms[submodel.parent];
        else
            _modelTransforms[sm] = Matrix::CreateFromQuaternion(rotation) * translation;
    }

    animation.elapsed = std::min(animation.elapsed + dt, animation.duration);
}

void PlayAnimation(const AnimationInstance& animation) {
    _animation = animation;
}

void SetKeyframe(int16 keyframe, int timeScale) {
    PlayAnimation({
        .from = keyframe,
        .to = keyframe,
        .duration = 0.16f,
        .timeScale = 1 / (float)timeScale
    });
}

void UpdateAnimations(ModelID modelId, float dt) {
    if (auto entry = g_ModelCache.Get(modelId)) {
        AnimateModel(entry->model, _animation, dt);
    }
}

void DrawMeshPrepass(GraphicsContext& context, const ModelInstance& instance) {
    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::modelPrepass);

    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto device = GetDevice();

    SetCommonShaderParameters(cmdList);

    auto entry = g_ModelCache.Get(instance.model);
    if (!entry) return;

    auto& model = entry->model;
    auto m = GetDeviceResources().meshPool->Get(entry->mesh);
    if (!m) return;
    auto& mesh = *m;

    ASSERT(mesh.submeshes.size() == model.submodels.size());

    for (int sm = 0; sm < entry->model.submodels.size(); ++sm) {
        auto& submesh = mesh.submeshes[sm];
        if (submesh.elementCount == 0) continue;
        auto& submodel = model.submodels[sm];
        // allocate three handles for the submesh

        if (HasFlag(submodel.flags, d3::SubmodelFlag::Alpha) ||
            HasFlag(submodel.flags, d3::SubmodelFlag::Additive) ||
            HasFlag(submodel.flags, d3::SubmodelFlag::Glow))
            continue;

        //auto submodelOffset = Vector3::Zero;
        //auto* smc = &submodel;
        //while (smc->parent != -1) {
        //    submodelOffset += smc->offset;
        //    smc = &model.submodels[smc->parent];
        //}

        // todo: these transforms could be shared between both passes. no need to upload twice.
        // the first handle is the object constants
        shaders::model::Constants constants = {};
        //auto translation = Matrix::CreateTranslation(submodelOffset);

        //Matrix::CreateRotationY((float)Clock.GetTotalTimeSeconds());
        //constants.world = Matrix::Identity * translation;
        constants.world = _modelTransforms[sm];

        if (HasFlag(submodel.flags, d3::SubmodelFlag::Facing)) {
            continue;
        }

        auto offset = frameBuffer.Copy(constants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
            .BufferLocation = frameBuffer->GetGPUVirtualAddress() + offset,
            .SizeInBytes = (uint)AlignTo(sizeof(shaders::model::Constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
        };

        // Allocate descriptors
        auto table = frameDescriptors.AllocateTable(2);

        // first is the texture handles
        device->CreateConstantBufferView(&desc, table.GetCpuHandle());

        // the second handle is the texture handles
        device->CreateShaderResourceView(mesh.textureHandles.Get(), &submesh.opaqueHandles, table.Offset(1).GetCpuHandle());

        cmdList->SetGraphicsRootDescriptorTable(3, table.GetGpuHandle()); // bind the table

        cmdList->IASetIndexBuffer(&submesh.opaqueIbv);
        cmdList->IASetVertexBuffers(0, 1, &submesh.vbv);
        cmdList->DrawIndexedInstanced(submesh.elementCount, 1, 0, 0, 0);
    }
}


void DrawMesh(GraphicsContext& context, const ModelInstance& instance) {
    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::model);

    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto device = GetDevice();

    SetCommonShaderParameters(cmdList);

    auto entry = g_ModelCache.Get(instance.model);
    if (!entry) return;

    auto& model = entry->model;
    auto m = GetDeviceResources().meshPool->Get(entry->mesh);
    if (!m) return;
    auto& mesh = *m;

    ASSERT(mesh.submeshes.size() == model.submodels.size());

    auto& spriteBatcher = GetSpriteBatcher();
    auto frame = GetFrameIndex();

    for (int sm = 0; sm < entry->model.submodels.size(); ++sm) {
        auto& submesh = mesh.submeshes[sm];
        if (submesh.elementCount == 0 && submesh.transparentElementCount == 0 && submesh.additiveElementCount == 0) continue;
        auto& submodel = model.submodels[sm];
        // allocate three handles for the submesh

        //auto submodelOffset = Vector3::Zero;
        //auto* smc = &submodel;
        //while (smc->parent != -1) {
        //    submodelOffset += smc->offset;
        //    smc = &model.submodels[smc->parent];
        //}

        // the first handle is the object constants
        //auto translation = Matrix::CreateTranslation(submodelOffset);

        //constants.world = Matrix::Identity * Matrix::CreateTranslation(submesh.model.offset) * Matrix::CreateRotationY((float)Clock.GetTotalTimeSeconds());
        // Matrix::CreateRotationY((float)Clock.GetTotalTimeSeconds())

        //constants.world = Matrix::Identity * rotation * translation;
        shaders::model::Constants constants = {};
        constants.world = _modelTransforms[sm];

        if (HasFlag(submodel.flags, d3::SubmodelFlag::Glow)) {
            // glows are basically sprites, but use a hard coded texture (thrustball.ogf)
            // the color and size are read from the submodel name

            auto& spriteBatch = spriteBatcher.GetSpriteBatch(BlendMode::Additive, frame);
            spriteBatch.Add({
                .vertex = {
                    .position = constants.world.Translation(),
                    .color = submodel.glow,
                    .size = Vector2(submodel.glowSize * 0.5f, submodel.glowSize * 0.5f),
                },
                // todo: depth is only needed for alpha sprites, not additive
                //.depth = Vector3::DistanceSquared(context.camera->Position, constants.world.Translation()),
                .depth = 0,
                .texture = submesh.texture
            });

            continue;
        }


        // todo: not all facing submodels are sprites?
        if (HasFlag(submodel.flags, d3::SubmodelFlag::Facing)) {
            ASSERT((int)submesh.texture >= 0);

            auto size = submodel.max - submodel.min;

            auto& spriteBatch = spriteBatcher.GetSpriteBatch(BlendMode::Additive, frame);
            spriteBatch.Add({
                .vertex = {
                    .position = constants.world.Translation(),
                    .color = Color(1, 1, 1, 1),
                    .size = Vector2(std::max(size.x, size.z) * 0.5f, size.y * 0.5f), // todo: there is a more robust function for this in D3 / D3edit
                },
                // todo: depth is only needed for alpha sprites, not additive
                //.depth = Vector3::DistanceSquared(context.camera->Position, constants.world.Translation()),
                .depth = 0,
                .texture = submesh.texture
            });
            continue;
        }


        //if (HasFlag(submodel.flags, d3::SubmodelFlag::Additive))
        //    continue;
        //if (HasFlag(submodel.flags, d3::SubmodelFlag::Alpha))
        //    continue;


        //if (HasFlag(submodel.flags, d3::SubmodelFlag::Additive))
        //    context.SetPipelineState(pipelines::modelAdditive);
        //else if (HasFlag(submodel.flags, d3::SubmodelFlag::Alpha))
        //    context.SetPipelineState(pipelines::modelAlpha);

        // instance constants
        auto offset = frameBuffer.Copy(constants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        D3D12_CONSTANT_BUFFER_VIEW_DESC constantsDesc = {
            .BufferLocation = frameBuffer->GetGPUVirtualAddress() + offset,
            .SizeInBytes = (uint)AlignTo(sizeof(shaders::model::Constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
        };

        cmdList->IASetVertexBuffers(0, 1, &submesh.vbv);

        if (submesh.elementCount > 0) {
            // Set up descriptors
            auto table = frameDescriptors.AllocateTable(2);
            device->CreateConstantBufferView(&constantsDesc, table.GetCpuHandle());
            device->CreateShaderResourceView(mesh.textureHandles.Get(), &submesh.opaqueHandles, table.Offset(1).GetCpuHandle());
            cmdList->SetGraphicsRootDescriptorTable(3, table.GetGpuHandle());

            context.SetPipelineState(pipelines::model);
            cmdList->IASetIndexBuffer(&submesh.opaqueIbv);
            cmdList->DrawIndexedInstanced(submesh.elementCount, 1, 0, 0, 0);
        }

        if (submesh.transparentElementCount > 0) {
            auto table = frameDescriptors.AllocateTable(2);
            device->CreateConstantBufferView(&constantsDesc, table.GetCpuHandle());
            device->CreateShaderResourceView(mesh.textureHandles.Get(), &submesh.alphaHandles, table.Offset(1).GetCpuHandle());
            cmdList->SetGraphicsRootDescriptorTable(3, table.GetGpuHandle());
            cmdList->IASetIndexBuffer(&submesh.transparentIbv);

            context.SetPipelineState(pipelines::modelAlphaBack);
            cmdList->DrawIndexedInstanced(submesh.transparentElementCount, 1, 0, 0, 0);

            context.SetPipelineState(pipelines::modelAlphaFront);
            cmdList->DrawIndexedInstanced(submesh.transparentElementCount, 1, 0, 0, 0);
        }

        if (submesh.additiveElementCount > 0) {
            auto table = frameDescriptors.AllocateTable(2);
            device->CreateConstantBufferView(&constantsDesc, table.GetCpuHandle());
            device->CreateShaderResourceView(mesh.textureHandles.Get(), &submesh.additiveHandles, table.Offset(1).GetCpuHandle());
            cmdList->SetGraphicsRootDescriptorTable(3, table.GetGpuHandle());

            context.SetPipelineState(pipelines::modelAdditive);
            cmdList->IASetIndexBuffer(&submesh.additiveIbv);
            cmdList->DrawIndexedInstanced(submesh.additiveElementCount, 1, 0, 0, 0);
        }
    }
}

}
