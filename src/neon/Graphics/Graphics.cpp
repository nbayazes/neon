#include "pch.h"
#include "Graphics.h"
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <spdlog/common.h>
#include "CommandContext.h"
#include "CommandQueue.h"
#include "DescriptorTable.h"
#include "DeviceResources.h"
#include "FrameConstants.h"
#include "imgui.h"
#include "imgui_local.h"
#include "Logging.h"
#include "neon-graphics.h"
#include "neon-types.h"
#include "Rml/RmlUI.h"
#include "ScopedTimer.h"
#include "shaders/compose.h"
#include "shaders/neon-shaders.h"
#include "Shell.h"
#include "SystemClock.h"
#include "Utility.h"
#include "Widechar.h"

namespace neon::gfx {

namespace {
    HWND _hwnd = nullptr;
    UINT _backBufferIndex = 0;
    UINT _backBufferCount = 2;
    uint _width = 1, _height = 1;
    uint64 _frame = 0;

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

void UpdateTextureInfo(const span<shaders::model::TextureInfo>& textures) {
    auto& uploadBuffer = resources.textureInfoUploadBuffer;
    uploadBuffer.Clear();

    auto device = GetDevice();
    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
    uploadContext.Reset();

    auto cmdList = uploadContext.GetCommandList();

    //auto sizeInBytes = GetVectorSizeInBytes(textures);

    // gpuMesh.textureMap.Create(fmt::format("{} TB{:02}", mesh.name, i), sizeInBytes);
    auto allocation = resources.textureInfo.Allocate(textures.size_bytes());
    uploadBuffer.Copy(textures);
    //uploadBuffer.Copy(textures, AlignTo(sizeof(shaders::model::TextureInfo), 256));
    uploadBuffer.CopyRegionTo(cmdList, resources.textureInfo, allocation.Offset, 0, textures.size_bytes());

    uploadContext.Execute();
    uploadContext.WaitForIdle();


    // update the descriptor after the upload is complete
    auto& desc = resources.textureInfoView;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = allocation.Offset / sizeof(shaders::model::TextureInfo);
    desc.Buffer.NumElements = (uint)textures.size();
    desc.Buffer.StructureByteStride = sizeof(shaders::model::TextureInfo);
}

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

#if GPU_VALIDATION
    // Enable GPU validation to find out of bounds resource access. VERY SLOW.
    ComPtr<ID3D12Debug1> spDebugController;
    if (SUCCEEDED(debugInterface->QueryInterface(IID_PPV_ARGS(&spDebugController))))
        spDebugController->SetEnableGPUBasedValidation(true);
#endif

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
    resources.graphicsQueue = make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Command Queue");

    //BatchUploadQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Batch Queue");
    //AsyncBatchUploadQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, "DeviceResources Batch Queue");
    //CopyQueue = make_unique<Inferno::CommandQueue>(m_d3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY, "DeviceResources Copy Queue");

    // Create a command allocator for each back buffer that will be rendered to.
    for (UINT n = 0; n < _backBufferCount; n++) {
        resources.graphicsContext[n] = make_unique<GraphicsContext>(resources.d3dDevice.Get(), resources.graphicsQueue.get(), fmt::format("Render target {}", n));
    }

    resources.shaderVisibleHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, "CBV/SRV/UAV heap");
    resources.renderTargetHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 20, "RTV heap");
    resources.depthStencilHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 20, "DSV heap");

    resources.reservedDescriptors = make_unique<DescriptorRange>(resources.shaderVisibleHeap.get(), 100);
    resources.sizedDescriptors = make_unique<DescriptorRange>(resources.shaderVisibleHeap.get(), 100);

    resources.sizedRenderTargetDescriptors = make_unique<DescriptorRange>(resources.renderTargetHeap.get(), 10);
    resources.renderTargetDescriptors = make_unique<DescriptorRange>(resources.renderTargetHeap.get(), 10);

    resources.sizedDepthStencilDescriptors = make_unique<DescriptorRange>(resources.depthStencilHeap.get());
    resources.depthStencilDescriptors = make_unique<DescriptorRange>(resources.depthStencilHeap.get());

    resources.frameDescriptors[0] = make_unique<DescriptorRange>(resources.shaderVisibleHeap.get(), 1000);
    resources.frameDescriptors[1] = make_unique<DescriptorRange>(resources.shaderVisibleHeap.get(), 1000);
    resources.textureDescriptors = make_unique<DescriptorRange>(resources.shaderVisibleHeap.get(), 20000);

    resources.copyQueue = std::make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_COPY, "texture copy queue");
    resources.textureCopyContext = std::make_unique<CommandContext>(device, resources.copyQueue.get(), "texture upload context");

    //resources.frameRingBuffer.Create("frame ring buffer", 1024 * 1024 * 1, BACK_BUFFER_COUNT);
    resources.frameBuffer[0].Create("frame ring buffer 0", 1024 * 1024 * 1, D3D12_HEAP_TYPE_UPLOAD);
    resources.frameBuffer[1].Create("frame ring buffer 1", 1024 * 1024 * 1, D3D12_HEAP_TYPE_UPLOAD);

    resources.textureInfo.Create("texture info", 1024 * 1024 * 1);

    resources.meshUploadBuffer.Create("Mesh upload buffer", 1024 * 1024 * 2, D3D12_HEAP_TYPE_UPLOAD);
    resources.textureInfoUploadBuffer.Create("Texture info upload buffer", 1024 * 1024 * 1, D3D12_HEAP_TYPE_UPLOAD);

    {
        // create white texture
        Image image;
        std::array data = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
        image.Load<uint32>(data, 2, 2);

        // todo: this should not be mixed with the other textures
        auto handle = CreateTexture(image, "white", true);
        resources.whiteTexture = GetTexture(handle);
        resources.reservedDescriptors->AddSRV(*resources.whiteTexture);
    }

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
    sizedResources.sceneColorBuffer.Create("Scene color buffer", width, height, DXGI_FORMAT_R11G11B10_FLOAT);
    sizedResources.sceneDepthBuffer.Create("Scene depth buffer", width, height);

    resources.sizedRenderTargetDescriptors->AddRTV(sizedResources.sceneColorBuffer);
    resources.sizedDescriptors->AddSRV(sizedResources.sceneColorBuffer);
    resources.sizedDepthStencilDescriptors->AddDSV(sizedResources.sceneDepthBuffer);

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
            resources.graphicsQueue->Get(),
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
        resources.sizedRenderTargetDescriptors->AddRTV(sizedResources.backBuffers[n]);
        //resources.renderTargetHeap->Allocate();
        //resources.renderTargetHeap->AddRTV(resources.backBuffers[n], n);
    }

    //if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN) {
    //    SceneDepthBuffer.Create("Depth stencil buffer", backBufferWidth, backBufferHeight, m_depthBufferFormat);
    //}

    // Reset the index to the current back buffer.
    _backBufferIndex = sizedResources.swapChain->GetCurrentBackBufferIndex();

    sizedResources.uiRenderTarget.Create("ui render target", width, height, pipelines::imgui.format, Color(0, 0, 0, 0));
    resources.sizedRenderTargetDescriptors->AddRTV(sizedResources.uiRenderTarget);
    resources.sizedDescriptors->AddSRV(sizedResources.uiRenderTarget);
}

//struct FrameAllocations {
//    UINT64 fenceValue = 0; // queue fence value recorded on submit
//    std::vector<D3D12MA::Allocation*> allocs;
//};

//FrameAllocations g_frameSlots[BACK_BUFFER_COUNT];

RenderTarget& GetBackBuffer() {
    return sizedResources.backBuffers[_backBufferIndex];
}

D3D12_GPU_VIRTUAL_ADDRESS& GetFrameConstants() {
    return resources.frameConstants[_frame % BACK_BUFFER_COUNT];
}

DescriptorRange& GetFrameDescriptors() {
    return *resources.frameDescriptors[_frame % BACK_BUFFER_COUNT];
}

GpuBuffer& GetFrameBuffer() {
    return resources.frameBuffer[_frame % BACK_BUFFER_COUNT];
}

void MoveToNextFrame() {
    _backBufferIndex = sizedResources.swapChain->GetCurrentBackBufferIndex();
    auto& nextFrame = resources.graphicsContext[_backBufferIndex];

    //auto& slot = g_frameSlots[_backBufferIndex];
    // free allocations from the ring buffer before starting the next frame
    //if (nextFrame->GetCommandQueue()->GetCompletedValue() >= slot.fenceValue) {
    //    for (auto* alloc : slot.allocs)
    //        alloc->Release();

    //    slot.allocs.clear();
    //    slot.fenceValue = 0;
    //}

    nextFrame->WaitForIdle(); // wait on the next frame to finish rendering before recording new commands
    _frame++;
    //auto fenceValue = nextFrame->GetCommandQueue()->GetCompletedValue();

    // Clear the per-frame resources for this frame
    GetFrameBuffer().Clear();
    GetFrameDescriptors().Clear();
}

GraphicsContext* GetGraphicsContext() {
    return resources.graphicsContext[_backBufferIndex].get();
}

void FreeResources() {
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

void Init(HWND hwnd, unsigned int width, unsigned int height, DeviceCreationOptions& options) {
    ASSERT(hwnd);
    _hwnd = hwnd;

    CreateDevice(options);
    CreateDeviceResources();
    CreateWindowSizeDependentResources(width, height);
    InitShaderCompiler(GetDevice());
    gfx::shaders::Compile();

    resources.states = make_unique<DirectX::CommonStates>(GetDevice());

    D3D12MA::Budget videoMemBudget = {};
    resources.memoryAllocator->GetBudget(&videoMemBudget, nullptr);

    SPDLOG_INFO("GPU memory usage {} / {} MB", videoMemBudget.UsageBytes / 1024 / 1024, videoMemBudget.BudgetBytes / 1024 / 1024);
}

void ReloadShaders() {
    resources.graphicsContext[0]->WaitForIdle();
    resources.graphicsContext[1]->WaitForIdle();
    gfx::shaders::Compile(true);
}

void ScreenSizeChanged(unsigned int width, unsigned int height) {
    resources.graphicsQueue->WaitForIdle();
    sizedResources = {};
    resources.sizedDescriptors->Clear();
    resources.sizedRenderTargetDescriptors->Clear();
    resources.sizedDepthStencilDescriptors->Clear();
    CreateWindowSizeDependentResources(width, height);
}

void Shutdown() {
    resources.graphicsQueue->WaitForIdle();
    FreeShaderCompiler();
    FreeResources();
}

constexpr auto VB_ALIGNMENT = 4;

void UpdateFrameConstants(const Camera& camera, float renderScale) {
    auto size = camera.GetViewportSize();

    FrameConstants frameConstants{};
    //frameConstants.ElapsedTime = Game::GetState() == GameState::MainMenu || Game::GetState() == GameState::Briefing
    //    ? (float)Inferno::Clock.GetTotalTimeSeconds()
    //    : (float)Game::Time;
    frameConstants.ElapsedTime = (float)Clock.GetTotalTimeSeconds();
    frameConstants.ViewProjection = camera.ViewProjection;
    frameConstants.View = camera.View;
    frameConstants.NearClip = camera.GetNearClip();
    frameConstants.FarClip = camera.GetFarClip();
    frameConstants.Eye = camera.Position;
    frameConstants.EyeDir = camera.GetForward();
    frameConstants.EyeUp = camera.Up;
    frameConstants.Size = Vector2{ size.x * renderScale, size.y * renderScale };
    frameConstants.RenderScale = renderScale;

    //auto offset = resources.frameRingBuffer.Copy(_frame, fenceValue, frameConstants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    //GetFrameConstants() = resources.frameRingBuffer->GetGPUVirtualAddress() + offset;
    auto& frameBuffer = GetFrameBuffer();
    auto offset = frameBuffer.Copy(frameConstants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    GetFrameConstants() = frameBuffer->GetGPUVirtualAddress() + offset;


    //frameConstants.GlobalDimming = Game::GlobalDimming;
    //frameConstants.NewLightMode = Settings::Graphics.NewLightMode && Settings::Editor.RenderMode == RenderMode::Shaded;
    //frameConstants.FilterMode = Settings::Graphics.FilterMode;

    //dest.ImmediateCopy(frameConstants);

    //dest.Begin();
    //dest.Copy({ &frameConstants, 1 });
    //dest.End();
}

void DrawMesh(GraphicsContext& context, const GpuMesh& mesh) {
    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::model);

    auto frameConstants = GetFrameConstants();
    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    // cmdList->SetGraphicsRootConstantBufferView(0, resources.meshes[0].textureData->GetGPUVirtualAddress());
    //cmdList->SetGraphicsRootConstantBufferView(0, resources.frameRingBuffer->GetGPUVirtualAddress());
    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //SPDLOG_INFO("Creating view at GPU address {} (offset {})", fmt::ptr((void*)(frameBuffer->GetGPUVirtualAddress() + offset)), offset);

    auto device = GetDevice();

    //Matrix transform = Matrix::CreateScale(object.Scale) * Matrix::Lerp(object.GetPrevTransform(), object.GetTransform(), Game::LerpAmount);

    //auto model = Resources::GetOutrageModel(object.Render.Model.ID);
    //if (model == nullptr) return;

    // Set the texture table
    cmdList->SetGraphicsRootDescriptorTable(2, resources.textureDescriptors->GetGpuHandle());

    // Set frame constants
    context.SetConstantBuffer(shaders::model::FrameConstants, frameConstants);

    for (auto& submesh : mesh.submeshes) {
        // allocate three handles for the submesh
        auto handle = frameDescriptors.GetNextHandle();
        frameDescriptors.Next();
        frameDescriptors.Next();

        auto submodelOffset = Vector3::Zero;
        auto* smc = &submesh.model;
        while (smc->parent != -1) {
            submodelOffset += smc->offset;
            smc = &mesh.model.submodels[smc->parent];
        }

        // the first handle is the object constants
        shaders::model::Constants constants = {};
        auto translation = Matrix::CreateTranslation(submodelOffset);

        //constants.world = Matrix::Identity * Matrix::CreateTranslation(submesh.model.offset) * Matrix::CreateRotationY((float)Clock.GetTotalTimeSeconds());
        constants.world = Matrix::Identity * translation * Matrix::CreateRotationY((float)Clock.GetTotalTimeSeconds());

        auto offset = frameBuffer.Copy(constants, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
            .BufferLocation = frameBuffer->GetGPUVirtualAddress() + offset,
            .SizeInBytes = (uint)AlignTo(sizeof(constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
        };

        // Create three consecutive descriptors
        device->CreateConstantBufferView(&desc, handle.GetCpuHandle());
        //cmdList->SetGraphicsRootDescriptorTable(2, resources.textureDescriptors->GetGpuHandle());

        // the second handle is the texture indices
        device->CreateShaderResourceView(mesh.textureIndices.Get(), &submesh.textureIndicesView, handle.Offset(1).GetCpuHandle());
        //cmdList->SetGraphicsRootDescriptorTable(shaders::model::TextureIndices, handle.Offset(1).GetGpuHandle());

        // the third handle is the texture table
        device->CreateShaderResourceView(resources.textureInfo.Get(), &resources.textureInfoView, handle.Offset(2).GetCpuHandle());

        // Three consecutive handles in the table
        cmdList->SetGraphicsRootDescriptorTable(1, handle.GetGpuHandle());


        cmdList->IASetIndexBuffer(&submesh.ibv);
        cmdList->IASetVertexBuffers(0, 1, &submesh.vbv);
        cmdList->DrawIndexedInstanced(submesh.elementCount, 1, 0, 0, 0);
    }
}

void Render(Camera& camera, RenderTarget& renderTarget, uint meshid) {
    camera.SetViewport({ shell::width, shell::height });
    camera.UpdatePerspectiveMatrices();
    camera.SetClipPlanes(0.1, 1000);


    auto& context = *resources.graphicsContext[_backBufferIndex];
    context.Reset();
    context.camera = &camera;
    context.SetViewportAndScissor(camera.GetViewportSize());

    float renderScale = 1;
    //auto fenceValue = context.GetCommandQueue()->GetNextValue();
    UpdateFrameConstants(camera, renderScale);

    auto cmdList = context.GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { resources.shaderVisibleHeap->Heap(), resources.states->Heap() };
    cmdList->SetDescriptorHeaps(std::size(heaps), heaps);

    context.SetRenderTarget(sizedResources.sceneColorBuffer, sizedResources.sceneDepthBuffer);
    Color background(0.05f, 0.05f, 0.05f);
    context.ClearRenderTarget(sizedResources.sceneColorBuffer, nullptr, &background);
    context.ClearDepth(sizedResources.sceneDepthBuffer);

    if (Seq::inRange(resources.meshes, meshid))
        DrawMesh(context, resources.meshes[meshid]);


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

void RenderView(Camera& camera, uint meshid) {
    auto& renderTarget = sizedResources.backBuffers[_backBufferIndex];
    Render(camera, renderTarget, meshid);
}

// Present the contents of the swap chain to the screen.
void Present() {
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
}

TexHandle CreateTexture(const Image& image, std::string_view name, bool reserved) {
    auto index = resources.textures.size();
    auto& texture = resources.textures.emplace_back();
    resources.textureCopyContext->Reset();
    auto intermediate = texture.Create(resources.textureCopyContext->GetCommandList(), image, name);
    resources.textureCopyContext->Execute();
    resources.textureCopyContext->WaitForIdle();

    if (reserved) {
        resources.reservedDescriptors->AddSRV(texture).ptr;
    }
    else {
        resources.textureDescriptors->AddSRV(texture).ptr;
    }

    return (TexHandle)index;
}

Texture* GetTexture(TexHandle index) {
    if (index >= resources.textures.size()) return nullptr;
    return &resources.textures[index];
}

void FreeTexture(TexHandle index) {
    // todo: this should queue the resource to be freed
    if (index >= resources.textures.size()) return;
    resources.textures[index] = {};
}

GpuMeshHandle CreateMesh() {
    return {};
}

uint64 CalculateMeshSize(const Mesh& mesh, uint64 alignment) {
    uint64 totalSize = 0;

    for (auto& submesh : mesh.submeshes) {
        totalSize += GetVectorSizeInBytes(submesh.vertices);
        totalSize = AlignTo(totalSize, alignment);

        totalSize += GetVectorSizeInBytes(submesh.indices);
        totalSize = AlignTo(totalSize, alignment);

        //totalSize += GetVectorSizeInBytes(submesh.textures);
        //totalSize = AlignTo(totalSize, alignment);
    }

    return totalSize;
}

uint64 CalculateTextureIndexSize(const Mesh& mesh) {
    uint64 totalSize = 0;

    for (auto& submesh : mesh.submeshes) {
        totalSize += GetVectorSizeInBytes(submesh.textureIndices);
        totalSize = AlignTo(totalSize, 4);
    }

    return totalSize;
}

std::mutex _uploadMutex;

void UploadMeshes(span<Mesh> meshes) {
    int64 time = 0;
    ScopedTimer timer(time);

    auto device = GetDevice();
    ASSERT(device);
    std::lock_guard lock(_uploadMutex);

    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
    uploadContext.Reset();

    auto cmdList = uploadContext.GetCommandList();

    auto& uploadBuffer = resources.meshUploadBuffer;
    uploadBuffer.Clear();

    // All buffers must use CBV alignment if they are packed in a single shared buffer

    for (int i = 0; i < meshes.size(); ++i) {
        auto& mesh = meshes[i];
        auto meshBufferSize = CalculateMeshSize(mesh, 4);

        auto& gpuMesh = resources.meshes.emplace_back();
        gpuMesh.meshData.Create(mesh.name, meshBufferSize);
        gpuMesh.textureIndices.Create(mesh.name + " texture indices", CalculateTextureIndexSize(mesh));
        gpuMesh.model = mesh.model;

        for (int j = 0; j < mesh.submeshes.size(); ++j) {
            auto& submesh = mesh.submeshes[j];
            if (submesh.vertices.size() == 0 || submesh.indices.size() == 0) continue;
            submesh.handle = (int)resources.meshes.size();

            auto& gpuSubmesh = gpuMesh.submeshes.emplace_back();
            gpuSubmesh.model = submesh.model;

            {
                auto sizeInBytes = GetVectorSizeInBytes(submesh.vertices);

                //gpuSubmesh.vertexBuffer.Create(fmt::format("{} VB{:02}", mesh.name, i), sizeInBytes);
                auto srcOffset = uploadBuffer.Copy(span{ submesh.vertices });
                auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
                //uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.vertexBuffer, 0, srcOffset, sizeInBytes);

                auto& vbv = gpuSubmesh.vbv;
                vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
                // vbv.BufferLocation = gpuSubmesh.vertexBuffer->GetGPUVirtualAddress();
                vbv.SizeInBytes = (uint)sizeInBytes;
                vbv.StrideInBytes = sizeof(shaders::ModelVertex);

                gpuSubmesh.elementCount = (uint)submesh.vertices.size();
            }

            {
                auto sizeInBytes = GetVectorSizeInBytes(submesh.indices);

                // gpuSubmesh.indexBuffer.Create(fmt::format("{} IB{:02}", mesh.name, i), sizeInBytes);
                auto srcOffset = uploadBuffer.Copy(span{ submesh.indices });
                auto allocation = gpuMesh.meshData.Allocate(sizeInBytes);
                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.meshData, allocation.Offset, srcOffset, sizeInBytes);
                // uploadBuffer.CopyRegionTo(cmdList, gpuSubmesh.indexBuffer, 0, srcOffset, sizeInBytes);

                auto& vbv = gpuSubmesh.ibv;
                vbv.BufferLocation = gpuMesh.meshData->GetGPUVirtualAddress() + allocation.Offset;
                // vbv.BufferLocation = gpuSubmesh.indexBuffer->GetGPUVirtualAddress();
                vbv.SizeInBytes = (uint)sizeInBytes;
                vbv.Format = DXGI_FORMAT_R16_UINT;
            }

            {
                auto sizeInBytes = GetVectorSizeInBytes(submesh.textureIndices);

                // gpuMesh.textureMap.Create(fmt::format("{} TB{:02}", mesh.name, i), sizeInBytes);
                auto allocation = gpuMesh.textureIndices.Allocate(sizeInBytes);
                auto srcOffset = uploadBuffer.Copy(span{ submesh.textureIndices });
                uploadBuffer.CopyRegionTo(cmdList, gpuMesh.textureIndices, allocation.Offset, srcOffset, sizeInBytes);

                auto& desc = gpuSubmesh.textureIndicesView;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                desc.Buffer.FirstElement = allocation.Offset / sizeof(int32);
                desc.Buffer.NumElements = (uint)submesh.textureIndices.size();
                desc.Buffer.StructureByteStride = sizeof(int32);
            }
        }
    }

    uploadContext.Execute();
    uploadContext.WaitForIdle();

    timer.Stop();
    SPDLOG_INFO("Model upload time: {:.2f} ms", time / 1000.0f);
}

}
