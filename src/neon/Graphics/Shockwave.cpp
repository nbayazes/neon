#include "pch.h"
#include "Shockwave.h"
#include "Device.h"
#include "Graphics.h"
#include "shaders/Shockwave.h"
#include "Utility.h"

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
        inner.color = Color(1, 1, 1, 0);
        inner.uv = Vector2(u, 0);

        auto& top = vertices.emplace_back();
        top.position = point * midOffset;
        top.position.y += thickness;
        top.color = Color(1, 1, 1);
        top.uv = Vector2(u, midV);

        auto& bottom = vertices.emplace_back();
        bottom.position = point * midOffset;
        bottom.position.y -= thickness;
        bottom.color = Color(1, 1, 1);
        bottom.uv = Vector2(u, midV);

        auto& outer = vertices.emplace_back();
        outer.position = point;
        outer.color = Color(1, 1, 1);
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

void DrawShockwave(GraphicsContext& context, const ShockwaveEffect& shockwave) {
    float t = shockwave.elapsed / shockwave.duration;
    if (t > 1) return;

    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::shockwave);

    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    auto device = GetDevice();

    SetCommonShaderParameters(cmdList);

    auto m = GetDeviceResources().meshPool->Get(gfx::shockwaveMesh);
    if (!m) return;
    auto& mesh = *m;

    for (int sm = 0; sm < mesh.submeshes.size(); ++sm) {
        auto& submesh = mesh.submeshes[sm];
        if (submesh.elementCount == 0 && submesh.transparentElementCount == 0 && submesh.additiveElementCount == 0) continue;

        Color color = shockwave.color;
        constexpr float shoulder = 0.35f;

        float st = (t - shoulder) / (1.0f - shoulder);
        color.w *= std::powf(1 - st, 2);
        float radius = shockwave.radius * (1.0f - std::powf(1 - t, 2));

        Matrix3x3 orientation = shockwave.rotation;

        // The mesh is built with Y+ as its normal, point that axis at the camera
        if (shockwave.faceCamera && context.camera) {
            Vector3 normal = context.camera->Position - shockwave.position;
            normal.Normalize();

            // Align the in-plane axes to the camera up so the screen rotation stays stable
            Vector3 right = context.camera->Up.Cross(normal);
            if (right.LengthSquared() < 1e-6f)
                right = context.camera->GetRight().Cross(normal);
            right.Normalize();
            Vector3 forward = right.Cross(normal);
            forward.Normalize();

            // Rotate the camera facing ring
            if (shockwave.cameraRotation != 0) {
                auto spin = Quaternion::CreateFromAxisAngle(normal, shockwave.cameraRotation);
                right = Vector3::Transform(right, spin);
                forward = Vector3::Transform(forward, spin);
            }

            orientation = Matrix3x3(right, normal, forward);
        }

        shaders::shockwave::Constants constants = {
            .transform = Matrix::CreateScale(radius) * orientation * Matrix::CreateTranslation(shockwave.position),
            .color = color,
            .texture = shockwave.texture,
        };

        // instance constants
        auto offset = frameBuffer.Copy(constants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        D3D12_CONSTANT_BUFFER_VIEW_DESC constantsDesc = {
            .BufferLocation = frameBuffer->GetGPUVirtualAddress() + offset,
            .SizeInBytes = (uint)AlignTo(sizeof(shaders::shockwave::Constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
        };

        cmdList->IASetVertexBuffers(0, 1, &submesh.vbv);

        //if (submesh.additiveElementCount > 0) {
        auto table = frameDescriptors.AllocateTable(1);
        device->CreateConstantBufferView(&constantsDesc, table.GetCpuHandle());
        cmdList->SetGraphicsRootDescriptorTable(3, table.GetGpuHandle());

        cmdList->IASetIndexBuffer(&submesh.additiveIbv);
        cmdList->DrawIndexedInstanced(submesh.additiveElementCount, 1, 0, 0, 0);
        //}
    }
}

}
