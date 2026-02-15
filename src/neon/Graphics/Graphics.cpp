#include "pch.h"

#include "neon-graphics.h"
#include "neon-types.h"

#include "Graphics.h"
#include <dxgi1_6.h>
#include <spdlog/common.h>
#include <D3D12MemAlloc.h>
#include "Logging.h"
#include "Widechar.h"
#include "DeviceResources.h"
#include <dxgidebug.h>
#include "CommandContext.h"
#include "CommandQueue.h"
#include "DescriptorTable.h"
#include "imgui.h"
#include "imgui_local.h"
#include "shaders/compose.h"
#include "shaders/neon-shaders.h"
#include "Shell.h"
#include "UploadBuffer.h"

namespace neon::gfx {
    namespace {
        //ComPtr<IDXGIFactory4> m_dxgiFactory;
        //ComPtr<IDXGISwapChain3> m_swapChain;
        //ComPtr<ID3D12Device> m_d3dDevice;

        //Ptr<CommandQueue> m_CommandQueue;
        //Ptr<DescriptorHeap> _shaderVisibleHeap;
        //Ptr<DescriptorHeap> _renderTargetHeap;
        //Ptr<DescriptorHeap> _depthStencilHeap;

        UploadBuffer<shaders::imgui::Vertex> VertexBuffer = { 3, "triangle vertices" };
        UploadBuffer<uint16> IndexBuffer = { 3, "triangle indices" };

        //bool _typedUAVLoadSupport_R11G11B10_FLOAT = false;
        float _renderScale = 1;
        //ComPtr<D3D12MA::Allocator> _allocator;
        HWND _hwnd = nullptr;
        UINT _backBufferIndex = 0;
        UINT _backBufferCount = 2;
        uint _width = 1, _height = 1;

        DeviceResources resources;
        WindowSizeResources sizedResources;

        DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT m_depthBufferFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        DWORD m_dxgiFactoryFlags = 0;

        DeviceCreationOptions m_options;

        constexpr DXGI_FORMAT StripSRGB(DXGI_FORMAT fmt) noexcept {
            switch (fmt) {
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
                case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;
                default: return fmt;
            }
        }
    }

    DeviceResources& GetDeviceResources() { return resources; }
    WindowSizeResources& GetWindowSizeResources() { return sizedResources; }

    D3D12MA::Allocator* GetMemoryAllocator() {
        return resources.memoryAllocator.Get();
    }

    ID3D12Device* GetDevice() { return resources.d3dDevice.Get(); }

    D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_12_0;

    void ReportLiveObjects() {
#ifdef _DEBUG
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
            //dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            std::ignore = dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        }
#endif
    }

    // This method acquires the first available hardware adapter that supports Direct3D 12.
    // If no such adapter can be found, try WARP. Otherwise throw an exception.
    void GetAdapter(IDXGIAdapter1** ppAdapter) {
        *ppAdapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
        ComPtr<IDXGIFactory6> factory6;
        HRESULT hr = resources.dxgiFactory.As(&factory6);
        if (SUCCEEDED(hr)) {
            for (UINT adapterIndex = 0;
                 SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                     adapterIndex,
                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                     IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
                 adapterIndex++) {
                DXGI_ADAPTER_DESC1 desc;
                ThrowIfFailed(adapter->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), minFeatureLevel, _uuidof(ID3D12Device), nullptr))) {
                    SPDLOG_INFO("Direct3D Adapter ({}): VID:{:x}, PID:{:x} - {}", adapterIndex, desc.VendorId, desc.DeviceId, Narrow(desc.Description));
                    break;
                }
            }
        }
#endif
        if (!adapter) {
            for (UINT adapterIndex = 0;
                 SUCCEEDED(resources.dxgiFactory->EnumAdapters1(
                     adapterIndex,
                     adapter.ReleaseAndGetAddressOf()));
                 ++adapterIndex) {
                DXGI_ADAPTER_DESC1 desc;
                ThrowIfFailed(adapter->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), minFeatureLevel, _uuidof(ID3D12Device), nullptr))) {
                    SPDLOG_INFO("Direct3D Adapter ({}): VID:{:x}, PID:{:x} - {}", adapterIndex, desc.VendorId, desc.DeviceId, Narrow(desc.Description));
                    break;
                }
            }
        }

#ifndef NDEBUG
        if (!adapter) {
            // Try WARP12 instead
            if (FAILED(resources.dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())))) {
                throw std::exception("WARP12 not available. Enable the 'Graphics Tools' optional feature");
            }

            OutputDebugStringA("Direct3D Adapter - WARP12\n");
        }
#endif

        if (!adapter)
            throw std::exception("No Direct3D 12 device found");

        *ppAdapter = adapter.Detach();
    }


    //#if defined(_DEBUG) && defined(GPU_DEBUG_LAYER)
    // Enable the debug layer (requires the Graphics Tools "optional feature" in Visual Studio).
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    DWORD EnableGpuDebugLayer() {
        //ComPtr<IDXGraphicsAnalysis> graphics_analysis;
        //const auto result = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&graphics_analysis));
        //if (FAILED(result)) {
        // Not running in PIX, enable the debug layer

        ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugInterface.GetAddressOf())))) {
            SPDLOG_INFO("Direct3D Debug Layer Enabled");
            debugInterface->EnableDebugLayer();
        }
        else {
            SPDLOG_WARN("Direct3D Debug Device is not available");
        }

        //#if GPU_VALIDATION
        // Enable GPU validation to find out of bounds resource access. VERY SLOW.
        //ComPtr<ID3D12Debug1> spDebugController1;
        //if (SUCCEEDED(debugInterface->QueryInterface(IID_PPV_ARGS(&spDebugController1))))
        //    spDebugController1->SetEnableGPUBasedValidation(true);
        //#endif

        // enable DRED to trace TDRs
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dreadSettings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dreadSettings)))) {
            // Turn on auto-breadcrumbs and page fault reporting.
            dreadSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dreadSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }

        // Enable breakpoints
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf())))) {
            std::ignore = dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            std::ignore = dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */
            DXGI_INFO_QUEUE_MESSAGE_ID hide[] = { 80 };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            std::ignore = dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
            return DXGI_CREATE_FACTORY_DEBUG;
        }

        return 0;
    }

    //#else
    //    DWORD EnableGpuDebugLayer() { return 0; }
    //#endif


    // Determines whether tearing support (variable refresh rate) is available for fullscreen borderless windows.
    bool CheckVRR() {
        BOOL allowTearing = false;

        ComPtr<IDXGIFactory5> factory5;
        HRESULT hr = resources.dxgiFactory.As(&factory5);
        if (SUCCEEDED(hr))
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

        if (FAILED(hr) || !allowTearing) {
            SPDLOG_WARN("Variable refresh rate is not supported");
            return false;
        }

        SPDLOG_INFO("Variable refresh rate is supported");
        return true;
    }

    bool CheckR11G11B10Load(ID3D12Device* device) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {
            DXGI_FORMAT_R11G11B10_FLOAT,
            D3D12_FORMAT_SUPPORT1_NONE,
            D3D12_FORMAT_SUPPORT2_NONE
        };

        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) &&
            (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0) {
            //_typedUAVLoadSupport_R11G11B10_FLOAT = true;
            SPDLOG_INFO("R11G11B10 UAV loading is supported");
            return true;
        }
        else {
            SPDLOG_WARN("R11G11B10 UAV loading is not supported");
            return false;
        }
    }

    D3D_FEATURE_LEVEL CheckFeatureLevel(ID3D12Device* device) {
        // Determine maximum supported feature level for this device
        static constexpr D3D_FEATURE_LEVEL s_featureLevels[] = {
            D3D_FEATURE_LEVEL_12_2, // Requires agility SDK on Windows 10
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels = {
            _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
        };

        // returns true when the feature level is supported
        std::ignore = SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels)));
        return featLevels.MaxSupportedFeatureLevel;
    }


    void ConfigureDebugMessages(ID3D12Device* device) {
#ifndef NDEBUG
        // Configure debug device (if active).
        ComPtr<ID3D12InfoQueue> d3dInfoQueue;

        if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12InfoQueue), &d3dInfoQueue))) {
#ifdef _DEBUG
            std::ignore = d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            std::ignore = d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D12_MESSAGE_ID hide[] = {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
                // Ignore perf warning about mismatched clear colors. The alternative fixes are worse.
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            std::ignore = d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
#endif
    }

    void CreateMemoryAllocator(ID3D12Device* device, IDXGIAdapter* adapter) {
        using namespace D3D12MA;
        ALLOCATOR_DESC desc = {
            .Flags = ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED,
            .pDevice = device,
            .PreferredBlockSize = 64 * 1024 * 1024,
            .pAdapter = adapter
        };

        if (FAILED(D3D12MA::CreateAllocator(&desc, resources.memoryAllocator.GetAddressOf()))) {
            throw std::exception("Unable to create D3D12MA::Allocator");
        }
    }


    void CreateDevice(DeviceCreationOptions& options) {
        // check if debugging can be enabled
        if (options.enableDebugging)
            m_dxgiFactoryFlags = EnableGpuDebugLayer();

        ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(resources.dxgiFactory.ReleaseAndGetAddressOf())));

        // check if VRR is actually available if it is requested
        if (options.allowTearing) {
            options.allowTearing = CheckVRR();
        }

        ComPtr<IDXGIAdapter1> adapter;
        GetAdapter(adapter.GetAddressOf());
        // Create the DX12 API device object.
        ThrowIfFailed(D3D12CreateDevice(
            adapter.Get(),
            minFeatureLevel,
            IID_PPV_ARGS(resources.d3dDevice.ReleaseAndGetAddressOf())
        ));

        std::ignore = resources.d3dDevice->SetName(L"D3D Device");

        ConfigureDebugMessages(resources.d3dDevice.Get());

        //auto typedLoadSupport = CheckR11G11B10Load(resources.d3dDevice.Get());
        auto featureLevel = CheckFeatureLevel(resources.d3dDevice.Get());
        if (featureLevel < D3D_FEATURE_LEVEL_12_0) {
            SPDLOG_ERROR("A DirectX 12 compatible GPU is required");
            throw Exception("A DirectX 12 compatible GPU is required");
        }

        CreateMemoryAllocator(resources.d3dDevice.Get(), adapter.Get());
    }

    void CreateDeviceResources() {
        // Create the command queues
        auto device = resources.d3dDevice.Get();
        resources.commandQueue = make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Command Queue");

        //BatchUploadQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Batch Queue");
        //AsyncBatchUploadQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Batch Queue");
        //CopyQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY, "DeviceResources Copy Queue");

        // Create a command allocator for each back buffer that will be rendered to.
        for (UINT n = 0; n < _backBufferCount; n++) {
            resources.graphicsContext[n] = make_unique<GraphicsContext>(resources.d3dDevice.Get(), resources.commandQueue.get(), fmt::format("Render target {}", n));
        }

        resources.shaderVisibleHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, "CBV/SRV/UAV heap");
        resources.renderTargetHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 20, "RTV heap");
        resources.depthStencilHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 20, "DSV heap");

        resources.reservedDescriptors = make_unique<LinearDescriptorRange>(resources.shaderVisibleHeap.get(), 100);
        resources.sizedDescriptors = make_unique<LinearDescriptorRange>(resources.shaderVisibleHeap.get(), 100);
        resources.textureDescriptors = make_unique<LinearDescriptorRange>(resources.shaderVisibleHeap.get(), 10000);

        resources.renderTargetDescriptors = make_unique<LinearDescriptorRange>(resources.renderTargetHeap.get());
        resources.depthStencilDescriptors = make_unique<LinearDescriptorRange>(resources.depthStencilHeap.get());

        neon::imgui::InitializeGraphics(_backBufferCount);
    }

    void WaitForGpu() {
        resources.graphicsContext[_backBufferIndex]->WaitForIdle();
    }


    void HandleDeviceLost();


    void CreateWindowSizeDependentResources(uint width, uint height, bool forceSwapChainRebuild = false) {
        WaitForGpu(); // Wait until all previous GPU work is complete.
        _width = width;
        _height = height;

        // Release resources that are tied to the swap chain and update fence values.
        for (UINT n = 0; n < _backBufferCount; n++) {
            sizedResources.backBuffers[n].Release();
            //m_fenceValues[n] = m_fenceValues[m_backBufferIndex];
        }

        // Determine the render target size in pixels.
        const auto backBufferWidth = std::max(width, 1u);
        const auto backBufferHeight = std::max(height, 1u);
        const DXGI_FORMAT backBufferFormat = StripSRGB(m_backBufferFormat);

        // creates intermediate render targets
        //CreateBuffers(backBufferWidth, backBufferHeight);

        // If the swap chain already exists, resize it, otherwise create one.
        if (sizedResources.swapChain && !forceSwapChainRebuild) {
            HRESULT hr = sizedResources.swapChain->ResizeBuffers(
                _backBufferCount,
                backBufferWidth,
                backBufferHeight,
                backBufferFormat,
                m_options.allowTearing && !m_options.useVsync ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
            );

            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                auto errorCode = uint(hr == DXGI_ERROR_DEVICE_REMOVED ? resources.d3dDevice->GetDeviceRemovedReason() : hr);
                SPDLOG_WARN("Device Lost on ResizeBuffers: Reason code {:#X}", errorCode);

                // If the device was removed for any reason, a new device and swap chain will need to be created.
                HandleDeviceLost();

                // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
                // and correctly set up the new device.
                return;
            }
            else {
                ThrowIfFailed(hr);
            }
        }
        else {
            if (sizedResources.swapChain)
                sizedResources.swapChain.Reset();

            // Create a descriptor for the swap chain.
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
                
                .Width = backBufferWidth,
                .Height = backBufferHeight,
                .Format = backBufferFormat,
                .SampleDesc = {
                    .Count = 1,
                    .Quality = 0,
                },
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = _backBufferCount,
                .Scaling = DXGI_SCALING_NONE, // use no scaling so resizing is smooth
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
                .Flags = m_options.allowTearing && !m_options.useVsync ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u,
            };

            DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
            fsSwapChainDesc.Windowed = TRUE;

            // Create a swap chain for the window.
            ComPtr<IDXGISwapChain1> swapChain;
            ThrowIfFailed(resources.dxgiFactory->CreateSwapChainForHwnd(
                resources.commandQueue->Get(),
                _hwnd,
                &swapChainDesc,
                &fsSwapChainDesc,
                nullptr,
                swapChain.GetAddressOf()
            ));

            ThrowIfFailed(swapChain.As(&sizedResources.swapChain));

            // prevent DXGI from responding to the ALT+ENTER shortcut
            //ThrowIfFailed(m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
        }

        // Handle color space settings for HDR
        //UpdateColorSpace();

        // Obtain the back buffers for this window which will be the final render targets
        // and create render target views for each of them.
        for (UINT n = 0; n < _backBufferCount; n++) {
            auto name = fmt::format("Back buffer {}", n);
            sizedResources.backBuffers[n].CreateBackBuffer(name, sizedResources.swapChain.Get(), n);
            resources.renderTargetDescriptors->AddRTV(sizedResources.backBuffers[n]);
            //resources.renderTargetHeap->Allocate();
            //resources.renderTargetHeap->AddRTV(resources.backBuffers[n], n);
        }

        //if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN) {
        //    SceneDepthBuffer.Create("Depth stencil buffer", backBufferWidth, backBufferHeight, m_depthBufferFormat);
        //}

        // Reset the index to the current back buffer.
        _backBufferIndex = sizedResources.swapChain->GetCurrentBackBufferIndex();

        sizedResources.uiRenderTarget = make_unique<RenderTarget>();
        sizedResources.uiRenderTarget->Create("ui render target", width, height, pipelines::imgui.format);
        resources.renderTargetDescriptors->AddRTV(*sizedResources.uiRenderTarget);
        resources.sizedDescriptors->AddSRV(*sizedResources.uiRenderTarget);
    }

    void MoveToNextFrame() {
        _backBufferIndex = sizedResources.swapChain->GetCurrentBackBufferIndex();
        auto& nextFrame = resources.graphicsContext[_backBufferIndex];
        nextFrame->WaitForIdle(); // wait on the next frame to finish rendering before recording new commands
    }

    GraphicsContext* GetGraphicsContext() {
        return resources.graphicsContext[_backBufferIndex].get();
    }

    void FreeResources() {
        imgui::FreeGraphics();
        sizedResources = {};
        resources = {};
        ReportLiveObjects();
    }

    // Recreate all device resources and set them back to the current state.
    void HandleDeviceLost() {
        FreeResources();
        CreateDeviceResources();
        CreateWindowSizeDependentResources(_width, _height);
    }


    //void CompilePipelineState(GraphicsPipelineInfo& effect, uint msaaSamples = 1, bool useStencil = true, uint renderTargets = 1) {
    //    try {
    //        auto psoDesc = BuildPipelineStateDesc(effect.settings, *effect.shader, useStencil, msaaSamples, renderTargets);
    //        ThrowIfFailed(resources.d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&effect.state)));
    //    }
    //    catch (const std::exception& e) {
    //        SPDLOG_ERROR("Unable to compile shader: {}", e.what());
    //    }
    //}

    void Init(HWND hwnd, unsigned int width, unsigned int height, DeviceCreationOptions& options) {
        ASSERT(hwnd);
        _hwnd = hwnd;

        CreateDevice(options);
        CreateDeviceResources();
        CreateWindowSizeDependentResources(width, height);
        InitShaderCompiler(GetDevice());
        gfx::shaders::Compile();

        resources.states = make_unique<DirectX::CommonStates>(GetDevice());
    }

    void ScreenSizeChanged(unsigned int width, unsigned int height) {
        sizedResources = {};
        resources.renderTargetDescriptors->ResetIndex();
        resources.sizedDescriptors->ResetIndex();
        CreateWindowSizeDependentResources(width, height);
    }

    void Shutdown() {
        FreeShaderCompiler();
        FreeResources();
    }

    RenderTarget& GetBackBuffer() {
        return sizedResources.backBuffers[_backBufferIndex];
    }

    void DrawTriangle(ID3D12GraphicsCommandList* cmdList) {
        shaders::imgui::Vertex triangleVertices[] = {
            { .position = { 10.0f, 10.0f }, .uv = { 0, 0 }, .color = 0xff0000 },
            { .position = { -10.0f, -10.0f }, .uv = { 0, 0 }, .color = 0x00ff00 },
            { .position = { -10.0f, 10.0f }, .uv = { 0, 0 }, .color = 0x0000ff }
        };

        VertexBuffer.Begin();
        VertexBuffer.Copy(triangleVertices);
        VertexBuffer.End();

        uint16 indices[] = { 0, 1, 2 };
        IndexBuffer.Begin();
        IndexBuffer.Copy(indices);
        IndexBuffer.End();


        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = VertexBuffer.GetGPUVirtualAddress();
        vbv.SizeInBytes = VertexBuffer.GetSizeInBytes();
        vbv.StrideInBytes = VertexBuffer.GetStride();
        cmdList->IASetVertexBuffers(0, 1, &vbv);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        //cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    void Render(GraphicsContext& context) {
        context.Reset();
        Color clearColor(0.05f, 0.05f, 0.05f);
        context.SetPipelineState(pipelines::imgui);
        context.SetViewportAndScissor({ shell::width, shell::height });

        auto cmdList = context.GetCommandList();

        ID3D12DescriptorHeap* heaps[] = { resources.shaderVisibleHeap->Heap(), resources.states->Heap() };
        cmdList->SetDescriptorHeaps(std::size(heaps), heaps);

        context.SetRenderTarget(*sizedResources.uiRenderTarget);

        neon::imgui::Draw();

        context.ClearColor(GetBackBuffer(), nullptr, &clearColor);
        context.SetRenderTarget(GetBackBuffer());

        sizedResources.uiRenderTarget->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        context.SetPipelineState(pipelines::compose);
        shaders::compose::SetSampler(cmdList, resources.states->PointClamp());
        shaders::compose::SetSource(cmdList, sizedResources.uiRenderTarget->GetSRV());
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    // Present the contents of the swap chain to the screen.
    void Present() {
        auto& context = resources.graphicsContext[_backBufferIndex];
        Render(*context);

        auto cmdList = context->GetCommandList();
        sizedResources.backBuffers[_backBufferIndex].Transition(cmdList, D3D12_RESOURCE_STATE_PRESENT);
        context->Execute();

        HRESULT hr{};
        if (m_options.allowTearing && !m_options.useVsync) {
            // Recommended to always use tearing if supported when using a sync interval of 0.
            // Note this will fail if in true 'fullscreen' mode.
            hr = sizedResources.swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        }
        else {
            // The first argument instructs DXGI to block until VSync, putting the application
            // to sleep until the next VSync. This ensures we don't waste any cycles rendering
            // frames that will never be displayed to the screen.
            hr = sizedResources.swapChain->Present(1, 0);
        }

        // If the device was reset we must completely reinitialize the renderer.
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
#ifdef _DEBUG
            auto errorCode = uint(hr == DXGI_ERROR_DEVICE_REMOVED ? resources.d3dDevice->GetDeviceRemovedReason() : hr);
            SPDLOG_WARN("Device Lost on Present: Reason code {:#X}", errorCode);
#endif
            HandleDeviceLost();
        }
        else {
            ThrowIfFailed(hr);

            MoveToNextFrame();

            if (!resources.dxgiFactory->IsCurrent()) {
                // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
                ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(resources.dxgiFactory.ReleaseAndGetAddressOf())));
            }
        }

        // Check for render scale change
        //if (Inferno::Settings::Graphics.RenderScale != _renderScale) {
        //    WaitForGpu();
        //    _renderScale = Inferno::Settings::Graphics.RenderScale;
        //    auto width = m_outputSize.right;
        //    auto height = m_outputSize.bottom;
        //    CreateBuffers(width, height);
        //}

        //Render::Allocator->SetCurrentFrameIndex(m_backBufferIndex);
    }
}
