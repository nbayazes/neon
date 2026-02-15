#pragma once
#include "neon-graphics.h"
#include <dxgi1_6.h>
#include "CommandContext.h"
#include "CommandQueue.h"
#include "DescriptorTable.h"
#include "shaders/imgui.h"
#include "Texture.h"
//#include "UploadBuffer.h"

namespace neon::gfx {
    using Microsoft::WRL::ComPtr;

    constexpr size_t MAX_BACK_BUFFER_COUNT = 2;

    // Resources alive for the duration of the device
    // NOTE: resources are destroyed in order
    struct DeviceResources {
        Ptr<CommandQueue> commandQueue;
        Ptr<DescriptorHeap> shaderVisibleHeap;
        Ptr<DescriptorHeap> renderTargetHeap;
        Ptr<DescriptorHeap> depthStencilHeap;

        Ptr<LinearDescriptorRange> reservedDescriptors; // Descriptors that are manually managed and live for the duration of the app
        Ptr<LinearDescriptorRange> textureDescriptors; // Descriptors for long lived textures
        Ptr<LinearDescriptorRange> sizedDescriptors; // Descriptors for resources that reset when window size changes
        Ptr<LinearDescriptorRange> renderTargetDescriptors; // RTV descriptors, resets when window size changes
        Ptr<LinearDescriptorRange> depthStencilDescriptors; // DSV descriptors, resets when window size changes

        Ptr<DirectX::CommonStates> states;

        ComPtr<D3D12MA::Allocator> memoryAllocator;

        Ptr<GraphicsContext> graphicsContext[MAX_BACK_BUFFER_COUNT];

        ComPtr<ID3D12Device> d3dDevice;
        ComPtr<IDXGIFactory4> dxgiFactory;

        //UploadBuffer<shaders::imgui::Vertex> triangleUploadBuffer = { 3, "triangle upload buffer" };
    };

    // Resources that are recreated when the window size changes
    struct WindowSizeResources {
        ComPtr<IDXGISwapChain3> swapChain;

        Ptr<RenderTarget> uiRenderTarget;
        RenderTarget backBuffers[MAX_BACK_BUFFER_COUNT];
        // frame buffers
    };


    // Gets the graphics context for the current frame
    D3D12MA::Allocator* GetMemoryAllocator();
    GraphicsContext* GetGraphicsContext();
    ID3D12Device* GetDevice();
    DeviceResources& GetDeviceResources();
    WindowSizeResources& GetWindowSizeResources();
}
