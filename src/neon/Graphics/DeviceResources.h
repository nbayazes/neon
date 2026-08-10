#pragma once
#include <dxgi1_6.h>
#include <directxtk12/CommonStates.h>
#include "CommandContext.h"
#include "CommandQueue.h"
#include "CycleBuffer.h"
#include "d3/OutrageModel.h"
#include "DescriptorTable.h"
#include "neon-graphics.h"
#include "shaders/Model.h"
#include "Texture.h"
//#include "UploadBuffer.h"

namespace neon::gfx {

using Microsoft::WRL::ComPtr;

// Back buffer count is also used as frames as flight, although technically not correct
constexpr size_t BACK_BUFFER_COUNT = 2;

struct GpuSubmesh {
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    D3D12_SHADER_RESOURCE_VIEW_DESC textureIndicesView{};
    uint elementCount = 0;
    d3::Submodel model{}; // HACK: remove asap
};

// GPU instanced mesh
struct GpuMesh {
    GpuBuffer meshData;
    GpuBuffer textureIndices;
    List<GpuSubmesh> submeshes;
    d3::Model model{}; // HACK: remove asap
    //StructuredBuffer textureMap; // buffer containing texture index for each geometry element
};

// Resources alive for the duration of the device
struct DeviceResources {
    Ptr<CommandQueue> graphicsQueue;
    Ptr<CommandQueue> copyQueue;
    Ptr<CommandContext> textureCopyContext;

    Ptr<DescriptorHeap> shaderVisibleHeap;
    Ptr<DescriptorHeap> renderTargetHeap;
    Ptr<DescriptorHeap> depthStencilHeap;

    Ptr<DescriptorRange> reservedDescriptors; // Descriptors that are manually managed and live for the duration of the app
    Ptr<DescriptorRange> textureDescriptors; // Descriptors for long lived textures
    Ptr<DescriptorRange> sizedDescriptors; // SRV descriptors, resets when window size changes
    Ptr<DescriptorRange> sizedRenderTargetDescriptors; // RTV descriptors, resets when window size changes
    Ptr<DescriptorRange> sizedDepthStencilDescriptors; // DSV descriptors, resets when window size changes
    Ptr<DescriptorRange> renderTargetDescriptors; // RTV descriptors
    Ptr<DescriptorRange> depthStencilDescriptors; // DSV descriptors
    Ptr<DescriptorRange> frameDescriptors[BACK_BUFFER_COUNT]; // per-frame descriptors. Resets after a new frame.

    Ptr<DirectX::CommonStates> states;

    // todo: split textures and meshes into separate resource groups based on type (menu, UI, level, object)
    List<Texture> textures;
    List<GpuMesh> meshes;
    Ptr<GraphicsContext> graphicsContext[BACK_BUFFER_COUNT];

    //GpuUploadBuffer frameConstants[BACK_BUFFER_COUNT];
    //FrameRingBuffer frameRingBuffer;
    GpuBuffer frameBuffer[BACK_BUFFER_COUNT]; // buffer for per-frame data

    gfx::GpuBuffer meshUploadBuffer;
    gfx::GpuBuffer textureInfoUploadBuffer;

    GpuBuffer textureInfo;
    D3D12_SHADER_RESOURCE_VIEW_DESC textureInfoView; // points at the latest copy of the texture info

    //GpuBuffer frameRingBuffer;

    D3D12_GPU_VIRTUAL_ADDRESS frameConstants[BACK_BUFFER_COUNT] = {};

    Texture* whiteTexture = nullptr;

    // NOTE: the memory allocator, device and factory must be last due to destruction order!

    ComPtr<D3D12MA::Allocator> memoryAllocator;
    ComPtr<ID3D12Device> d3dDevice;
    ComPtr<IDXGIFactory4> dxgiFactory;
};

// Resources that are recreated when the window size changes
struct WindowSizeResources {
    ComPtr<IDXGISwapChain3> swapChain;

    RenderTarget uiRenderTarget;
    RenderTarget backBuffers[BACK_BUFFER_COUNT];
    // frame buffers
    RenderTarget sceneColorBuffer;
    DepthBuffer sceneDepthBuffer;
};


D3D12MA::Allocator* GetMemoryAllocator();

// Gets the graphics context for the current frame
GraphicsContext* GetGraphicsContext();
ID3D12Device* GetDevice();
DeviceResources& GetDeviceResources();
WindowSizeResources& GetWindowSizeResources();

}
