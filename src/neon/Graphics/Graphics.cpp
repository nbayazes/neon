#include "pch.h"
#include "Graphics.h"
#include "CommandContext.h"
#include "DescriptorTable.h"
#include "Device.h"
#include "DrawCommand.h"
#include "FrameConstants.h"
#include "imgui_local.h"
#include "Mesh.h"
#include "neon-graphics.h"
#include "neon-types.h"
#include "Particles.h"
#include "Rml/RmlUI.h"
#include "shaders/compose.h"
#include "shaders/neon-shaders.h"
#include "Shell.h"
#include "Shockwave.h"
#include "SpriteBatch.h"
#include "SystemClock.h"
#include "Utility.h"

namespace neon::gfx {

void UpdateTextureInfo(const span<TextureInfo>& textures) {
    if (textures.size_bytes() == 0) return;

    auto& resources = GetDeviceResources();
    auto& uploadBuffer = resources.textureInfoUploadBuffer;
    uploadBuffer.Clear();

    auto device = GetDevice();
    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
    uploadContext.Reset();

    auto cmdList = uploadContext.GetCommandList();

    // gpuMesh.textureMap.Create(fmt::format("{} TB{:02}", mesh.name, i), sizeInBytes);
    auto allocation = resources.textureInfo.Allocate(textures.size_bytes());
    uploadBuffer.CopyRange(textures);
    //uploadBuffer.Copy(textures, AlignTo(sizeof(shaders::model::TextureInfo), 256));
    uploadBuffer.CopyRegionTo(cmdList, resources.textureInfo, allocation.Offset, 0, textures.size_bytes());

    uploadContext.Execute();
    uploadContext.WaitForIdle();


    // update the descriptor after the upload is complete
    auto& desc = resources.textureInfoView;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = allocation.Offset / sizeof(TextureInfo);
    desc.Buffer.NumElements = (uint)textures.size();
    desc.Buffer.StructureByteStride = sizeof(TextureInfo);
}

constexpr auto VB_ALIGNMENT = 4;

void UpdateFrameConstants(const Camera& camera, float renderScale) {
    auto& resources = GetDeviceResources();
    auto size = camera.GetViewportSize();

    FrameConstants frameConstants{};
    //frameConstants.ElapsedTime = Game::GetState() == GameState::MainMenu || Game::GetState() == GameState::Briefing
    //    ? (float)Inferno::Clock.GetTotalTimeSeconds()
    //    : (float)Game::Time;
    frameConstants.ElapsedTime = (float)Clock.GetTotalTimeSeconds();
    frameConstants.ViewProjection = camera.ViewProjection;
    frameConstants.View = camera.View;
    frameConstants.Projection = camera.Projection;
    frameConstants.NearClip = camera.GetNearClip();
    frameConstants.FarClip = camera.GetFarClip();
    frameConstants.Eye = camera.Position;
    frameConstants.EyeDir = camera.GetForward();
    frameConstants.EyeUp = camera.Up;
    frameConstants.Size = Vector2{ size.x * renderScale, size.y * renderScale };
    frameConstants.RenderScale = renderScale;

    auto& frameBuffer = GetFrameBuffer();
    frameBuffer.Clear();
    auto offset = frameBuffer.Copy(frameConstants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GetFrameConstants() = frameBuffer->GetGPUVirtualAddress() + offset;

    auto device = GetDevice();
    auto commonTable = GetFrameDescriptors().AllocateTable(2);

    D3D12_CONSTANT_BUFFER_VIEW_DESC frameConstantsDesc = {
        .BufferLocation = frameBuffer->GetGPUVirtualAddress() + offset,
        .SizeInBytes = (uint)AlignTo(sizeof(FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
    };

    device->CreateConstantBufferView(&frameConstantsDesc, commonTable.GetCpuHandle());
    if (resources.textureInfoView.Buffer.NumElements > 0)
        device->CreateShaderResourceView(resources.textureInfo.Get(), &resources.textureInfoView, commonTable.Offset(1).GetCpuHandle());

    GetCommonDescriptorTable() = commonTable;
    resources.TextureInfoDescriptor = commonTable.Offset(1);
}

void SetCommonShaderParameters(ID3D12GraphicsCommandList* cmdList) {
    auto& resources = GetDeviceResources();
    cmdList->SetGraphicsRootConstantBufferView(0, GetFrameConstants());
    cmdList->SetGraphicsRootDescriptorTable(1, resources.textureDescriptors->GetGpuHandle());
    cmdList->SetGraphicsRootDescriptorTable(2, resources.TextureInfoDescriptor.GetGpuHandle());
}

void ExecuteDrawCommand(GraphicsContext& context, const DrawCommand& command, RenderPass pass) {
    auto& resources = GetDeviceResources();
    auto cmdList = context.GetCommandList();

    switch (command.type) {
        case DrawCommandType::Mesh:
            ASSERT(command.indexBuffer.BufferLocation);
            ASSERT(command.vertexBuffer.BufferLocation);

            if (pass == RenderPass::Additive)
                context.SetPipelineState(pipelines::modelAdditive);
            else if (pass == RenderPass::Transparent)
                context.SetPipelineState(pipelines::modelAlphaBack);
            else
                context.SetPipelineState(pipelines::model);

            context.SetPipelineState(pass == RenderPass::Opaque ? pipelines::model : pipelines::modelAdditive);

            SetCommonShaderParameters(cmdList);
            // Set the texture table
            //cmdList->SetGraphicsRootDescriptorTable(2, resources.textureDescriptors->GetGpuHandle());

            // Set frame constants
            //context.SetConstantBuffer(0, GetFrameConstants());

            // Three consecutive handles in the table
            cmdList->SetGraphicsRootDescriptorTable(1, command.descriptorTable);

            cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList->IASetIndexBuffer(&command.indexBuffer);
            cmdList->IASetVertexBuffers(0, 1, &command.vertexBuffer);
            cmdList->DrawIndexedInstanced(command.count, 1, 0, 0, 0);

            if (pass == RenderPass::Transparent) {
                context.SetPipelineState(pipelines::modelAlphaFront);
                cmdList->DrawIndexedInstanced(command.count, 1, 0, 0, 0);
            }

            break;

        case DrawCommandType::Sprite:
            ASSERT(command.vertexBuffer.BufferLocation);

            context.SetPipelineState(pass == RenderPass::Additive ? pipelines::spriteAdditive : pipelines::sprite);

            cmdList->SetGraphicsRootDescriptorTable(0, resources.textureDescriptors->GetGpuHandle());

            cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList->IASetVertexBuffers(0, 1, &command.vertexBuffer);
            cmdList->DrawInstanced(3, command.count * 2, 0, 0);
            break;
    }
}

void Render(Camera& camera, RenderTarget& renderTarget, const Scene& scene, float tickAlpha) {
    auto& resources = GetDeviceResources();
    auto& sizedResources = GetWindowSizeResources();
    auto& spriteBatcher = GetSpriteBatcher();
    auto frame = GetFrameIndex();

    camera.SetViewport({ shell::width, shell::height });
    camera.UpdatePerspectiveMatrices();
    camera.SetClipPlanes(1, 1000);

    auto& context = *GetGraphicsContext();
    context.Reset();
    context.camera = &camera;
    context.SetViewportAndScissor(camera.GetViewportSize());

    float renderScale = 1;
    //auto fenceValue = context.GetCommandQueue()->GetNextValue();
    UpdateFrameConstants(camera, renderScale);

    auto cmdList = context.GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { resources.shaderVisibleHeap->Heap(), resources.states->Heap() };
    cmdList->SetDescriptorHeaps(std::size(heaps), heaps);

    // depth prepass
    context.SetRenderTarget(sizedResources.linearDepthBuffer, sizedResources.sceneDepthBuffer);
    context.ClearRenderTarget(sizedResources.linearDepthBuffer, nullptr);
    context.ClearDepth(sizedResources.sceneDepthBuffer);

    for (auto& model : scene.models)
        DrawMeshPrepass(context, model);

    // opaque pass
    context.SetRenderTarget(sizedResources.sceneColorBuffer, sizedResources.sceneDepthBuffer);
    //Color background(0.70f, 0.70f, 0.70f);
    Color background(0.10f, 0.10f, 0.10f);
    context.ClearRenderTarget(sizedResources.sceneColorBuffer, nullptr, &background);
    context.ClearDepth(sizedResources.sceneDepthBuffer);

    for (auto& model : scene.models)
        DrawMesh(context, model);

    for (auto& system : scene.particles)
        DrawParticleSystem(context, system, tickAlpha);

    // additive pass
    spriteBatcher.Draw(context, BlendMode::Alpha, frame);
    spriteBatcher.Draw(context, BlendMode::Additive, frame);
    spriteBatcher.Draw(context, BlendMode::Multiply, frame);

    for (auto& shockwave : scene.shockwaves) {
        DrawShockwave(context, shockwave);
    }

    context.SetRenderTarget(sizedResources.uiRenderTarget);
    context.ClearRenderTarget(sizedResources.uiRenderTarget, nullptr);

    neon::rml::Draw();
    neon::imgui::Draw();

    context.SetViewportAndScissor({ shell::width, shell::height });
    Color clearColor(0.05f, 0.05f, 0.05f);
    context.ClearRenderTarget(GetBackBuffer(), nullptr, &clearColor);
    context.SetRenderTarget(GetBackBuffer());

    sizedResources.uiRenderTarget.Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.SetPipelineState(pipelines::compose);
    context.SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    shaders::compose::SetSampler(cmdList, resources.states->PointClamp());

    sizedResources.sceneColorBuffer.Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    shaders::compose::SetSource(cmdList, sizedResources.sceneColorBuffer.GetSRV());
    cmdList->DrawInstanced(3, 1, 0, 0);

    shaders::compose::SetSource(cmdList, sizedResources.uiRenderTarget.GetSRV());
    cmdList->DrawInstanced(3, 1, 0, 0);

    // execute the command list
    renderTarget.Transition(cmdList, D3D12_RESOURCE_STATE_PRESENT);
    context.Execute();
}

void RenderView(Camera& camera, const Scene& scene, float tickAlpha) {
    Render(camera, GetBackBuffer(), scene, tickAlpha);
}

void Update() {}

}
