#include "pch.h"
#include "Graphics.h"
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <spdlog/common.h>
#include <algorithm>
#include "CommandContext.h"
#include "CommandQueue.h"
#include "DescriptorTable.h"
#include "DeviceResources.h"
#include "DrawCommand.h"
#include "FrameConstants.h"
#include "imgui.h"
#include "imgui_local.h"
#include "Logging.h"
#include "MeshPool.h"
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
#include "ModelCache.h"
#include "ShaderCompiler.h"
#include "shaders/imgui.h"
#include "shaders/ModelPrepass.h"
#include "shaders/Sprite.h"

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

RenderTarget& GetBackBuffer() {
    return sizedResources.backBuffers[_backBufferIndex];
}

D3D12_GPU_VIRTUAL_ADDRESS& GetFrameConstants() {
    return resources.frameConstants[_frame % BACK_BUFFER_COUNT];
}

DescriptorRange& GetFrameDescriptors() {
    return *resources.frameDescriptors[_frame % BACK_BUFFER_COUNT];
}

DescriptorHandle& GetCommonDescriptorTable() {
    return resources.CommonShaderTable[_frame % BACK_BUFFER_COUNT];
}

GpuBuffer& GetFrameBuffer() {
    return resources.frameBuffer[_frame % BACK_BUFFER_COUNT];
}


struct SpriteBatchInfo {
    shaders::sprite::Vertex vertex;
    float depth;
    TexID texture;
};

class SpriteBatch {
    List<SpriteBatchInfo> _sprites;
    GpuBuffer _vertices;
    GpuBuffer _textureHandles;

    List<shaders::sprite::Vertex> _stagingVertexBuffer;
    List<TexID> _stagingTextureHandles;

public:
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC textureViewDesc = {};

    void Create(uint64 capacity) {
        _vertices.Create("sprite batch", sizeof(shaders::sprite::Vertex) * capacity, D3D12_HEAP_TYPE_UPLOAD);
        _textureHandles.Create("sprite batch handles", sizeof(int32) * capacity, D3D12_HEAP_TYPE_UPLOAD);
        //_vertices.Unmap();
        //auto& resources = GetDeviceResources();
        //_uploadContext = make_unique<gfx::CommandContext>(GetDevice(), resources.copyQueue.get(), "Sprite upload command list");
        //_uploadContext->Reset();

        _stagingVertexBuffer.reserve(capacity / 2);
        _stagingTextureHandles.reserve(capacity / 2);
        _sprites.reserve(capacity / 2);

        //D3D12_VERTEX_BUFFER_VIEW dummyVBV = {};
        //dummyVBV.BufferLocation = _vertices->GetGPUVirtualAddress();
        //dummyVBV.StrideInBytes = 1;
        //dummyVBV.SizeInBytes = 1;
    }

    void Add(const SpriteBatchInfo& sprite) {
        _sprites.push_back(sprite);
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetTextureHandleAddress() {
        return _textureHandles->GetGPUVirtualAddress();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetVerticesAddress() {
        return _vertices->GetGPUVirtualAddress();
    }

    //void Clear() {
    //    _sprites.clear();
    //}

    //void Sort() {
    //    Seq::sortBy(_sprites, [](auto& a, auto& b) { return a.depth < b.depth; });
    //}

    //span<SpriteBatchInfo> Sprites() { return _sprites; }

    void Upload() {
        Count = (uint)_sprites.size();
        if (Count == 0) return;

        Seq::sortBy(_sprites, [](auto& a, auto& b) { return a.depth < b.depth; });

        _stagingVertexBuffer.clear();
        _stagingTextureHandles.clear();

        _stagingVertexBuffer.resize(_sprites.size());
        _stagingTextureHandles.resize(_sprites.size());

        for (int i = 0; i < _sprites.size(); ++i) {
            _stagingVertexBuffer[i] = _sprites[i].vertex;
            _stagingTextureHandles[i] = _sprites[i].texture;
        }

        _vertices.Clear();
        _vertices.CopyRange(span{ _stagingVertexBuffer });

        _textureHandles.Clear();
        _textureHandles.CopyRange(span{ _stagingTextureHandles });

        vertexBufferView = {
            .BufferLocation = _vertices->GetGPUVirtualAddress(),
            .SizeInBytes = (uint)GetVectorSizeInBytes(_stagingVertexBuffer) * 4,
            .StrideInBytes = sizeof(shaders::sprite::Vertex)
        };

        //vertexBufferView = {
        //    .BufferLocation = _vertices->GetGPUVirtualAddress(),
        //    .SizeInBytes = 4,
        //    .StrideInBytes = 4
        //};

        textureViewDesc.Format = DXGI_FORMAT_UNKNOWN;
        textureViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        textureViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        textureViewDesc.Buffer.FirstElement = 0;
        textureViewDesc.Buffer.NumElements = (uint)_sprites.size();
        textureViewDesc.Buffer.StructureByteStride = sizeof(int32);

        _sprites.clear();
    }

    uint Count = 0;

    //void Upload(const shaders::sprite::Vertex& vertex, TexID texture) {
    //    //_sizeInBytes += 
    //    _vertices.CopyRaw(vertex, _vertexOffset);
    //    _vertexOffset += sizeof(vertex);

    //    // todo: texture handles
    //    _textureHandles.CopyRaw(texture, _handleOffset);
    //    _handleOffset += sizeof(int);
    //}

    //D3D12_VERTEX_BUFFER_VIEW CreateView() {
    //    D3D12_VERTEX_BUFFER_VIEW vbv{};
    //    vbv.BufferLocation = _vertices->GetGPUVirtualAddress();
    //    // vbv.BufferLocation = gpuSubmesh.vertexBuffer->GetGPUVirtualAddress();
    //    vbv.SizeInBytes = GetVectorSizeInBytes(_stagingVertexBuffer);
    //    vbv.StrideInBytes = sizeof(shaders::sprite::Vertex);

    //    _uploadContext->Reset();
    //    _sizeInBytes = 0;
    //}
};

SpriteBatch g_SpriteBatch[BACK_BUFFER_COUNT];

SpriteBatch& GetSpriteBatch() {
    return g_SpriteBatch[_frame % BACK_BUFFER_COUNT];
}

DeviceResources& GetDeviceResources() { return resources; }
WindowSizeResources& GetWindowSizeResources() { return sizedResources; }

D3D12MA::Allocator* GetMemoryAllocator() {
    return resources.memoryAllocator.Get();
}

ID3D12Device* GetDevice() { return resources.d3dDevice.Get(); }

void UpdateTextureInfo(const span<TextureInfo>& textures) {
    auto& uploadBuffer = resources.textureInfoUploadBuffer;
    uploadBuffer.Clear();

    auto device = GetDevice();
    gfx::CommandContext uploadContext = { device, resources.copyQueue.get(), "Mesh upload command list" };
    uploadContext.Reset();

    auto cmdList = uploadContext.GetCommandList();

    //auto sizeInBytes = GetVectorSizeInBytes(textures);

    if (textures.size_bytes() == 0) return;

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

    resources.textureInfo.Create("texture info", sizeof(TextureInfo) * 5000);

    resources.meshUploadBuffer.Create("Mesh upload buffer", 1024 * 1024 * 2, D3D12_HEAP_TYPE_UPLOAD);
    resources.textureInfoUploadBuffer.Create("Texture info upload buffer", 1024 * 1024 * 1, D3D12_HEAP_TYPE_UPLOAD);
    resources.meshPool = std::make_unique<MeshPool>();

    for (auto& batch : g_SpriteBatch) {
        batch.Create(1024);
    }

    {
        // create white texture
        Image image;
        std::array data = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
        image.Load<uint32>(data, 2, 2);

        // todo: this should not be mixed with the other textures
        auto handle = UploadTexture(image, "white", true);
        resources.whiteTexture = GetTexture(handle, true);
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

    sizedResources.linearDepthBuffer.Create("linear depth buffer", width, height, LINEAR_DEPTH_FORMAT, Color(0, 0, 0, 0));
    resources.sizedRenderTargetDescriptors->AddRTV(sizedResources.linearDepthBuffer);
    resources.sizedDescriptors->AddSRV(sizedResources.linearDepthBuffer);
}

//struct FrameAllocations {
//    UINT64 fenceValue = 0; // queue fence value recorded on submit
//    std::vector<D3D12MA::Allocation*> allocs;
//};

//FrameAllocations g_frameSlots[BACK_BUFFER_COUNT];

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
    for (auto& batch : g_SpriteBatch) {
        batch = {};
    }

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

    // the second handle is the texture indices
    //device->CreateShaderResourceView(resources.textures.), &submesh.textureIndicesView, commonTable.Offset(1).GetCpuHandle());
    //cmdList->SetGraphicsRootDescriptorTable(shaders::model::TextureIndices, handle.Offset(1).GetGpuHandle());

    // the third handle is the texture table
    //device->CreateShaderResourceView(resources.textureInfo.Get(), &resources.textureInfoView, commonTable.Offset(2).GetCpuHandle());

    //frameConstants.GlobalDimming = Game::GlobalDimming;
    //frameConstants.NewLightMode = Settings::Graphics.NewLightMode && Settings::Editor.RenderMode == RenderMode::Shaded;
    //frameConstants.FilterMode = Settings::Graphics.FilterMode;

    //dest.ImmediateCopy(frameConstants);

    //dest.Begin();
    //dest.Copy({ &frameConstants, 1 });
    //dest.End();
}

void SetCommonShaderParmeters(ID3D12GraphicsCommandList* cmdList) {
    cmdList->SetGraphicsRootConstantBufferView(0, GetFrameConstants());
    cmdList->SetGraphicsRootDescriptorTable(1, resources.textureDescriptors->GetGpuHandle());
    cmdList->SetGraphicsRootDescriptorTable(2, resources.TextureInfoDescriptor.GetGpuHandle());
}

void DrawSprites(GraphicsContext& context) {
    auto& sprites = GetSpriteBatch();
    if (sprites.Count == 0) return;

    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::spriteAdditive);

    SetCommonShaderParmeters(cmdList);
    cmdList->SetGraphicsRootShaderResourceView(3, sprites.GetTextureHandleAddress());
    cmdList->SetGraphicsRootShaderResourceView(4, sprites.GetVerticesAddress());

    sizedResources.linearDepthBuffer.Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    //cmdList->SetGraphicsRootShaderResourceView(5, sizedResources.linearDepthBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(5, sizedResources.linearDepthBuffer.GetSRV());
    //device->CreateShaderResourceView(resources.textureInfo.Get(), &resources.textureInfoView, handle.Offset(2).GetCpuHandle());

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->DrawInstanced(4, sprites.Count, 0, 0);
}

void ExecuteDrawCommand(GraphicsContext& context, const DrawCommand& command, RenderPass pass) {
    auto cmdList = context.GetCommandList();

    switch (command.type) {
        case DrawCommandType::Mesh:
            ASSERT(command.indexBuffer.BufferLocation);
            ASSERT(command.vertexBuffer.BufferLocation);

            if (pass == RenderPass::Additive)
                context.SetPipelineState(pipelines::modelAdditive);
            else if (pass == RenderPass::Transparent)
                context.SetPipelineState(pipelines::modelAlpha);
            else
                context.SetPipelineState(pipelines::model);

            context.SetPipelineState(pass == RenderPass::Opaque ? pipelines::model : pipelines::modelAdditive);

            SetCommonShaderParmeters(cmdList);
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

AnimationInstance _animation;

// Accumulated transforms for a model
List<Matrix> _modelTransforms;

void AnimateModel(d3::Model& model, AnimationInstance& animation, float dt) {
    _modelTransforms.resize(model.submodels.size());

    if (animation.elapsed >= animation.duration) {
        // finished playing. don't update
        return;
    }


    float percent = animation.elapsed / animation.duration;
    int range = animation.to - animation.from;
    float frameDuration = animation.duration / range;
    float alpha = std::fmod(animation.elapsed, frameDuration) / frameDuration;  // percentage of current frame to next
    
    int16 startFrame = int16(animation.from + range * percent);
    int16 endFrame = std::min(int16(startFrame + 1), animation.to);

    SPDLOG_INFO("frame: {} - {} alpha: {}", startFrame, endFrame, alpha);

    for (int sm = 0; sm < model.submodels.size(); ++sm) {
        auto& submodel = model.submodels[sm];

        Quaternion rotation = Quaternion::Identity;

        if (Seq::inRange(submodel.keyframes, startFrame) && Seq::inRange(submodel.keyframes, endFrame)) {
            auto& start = submodel.keyframes[startFrame];
            auto& end = submodel.keyframes[endFrame];
            rotation = Quaternion::Slerp(start.rotation, end.rotation, alpha);
        }

        Vector3 position = Vector3::Zero;
        if (Seq::inRange(submodel.positionKeyframes, startFrame) && Seq::inRange(submodel.positionKeyframes, endFrame)) {
            auto& start = submodel.positionKeyframes[startFrame];
            auto& end = submodel.positionKeyframes[endFrame];
            position = Vector3::Lerp(start.position, end.position, alpha);
        }
        
        auto translation = Matrix::CreateTranslation(submodel.offset + position);

        if (submodel.parent >= 0) {
            _modelTransforms[sm] = Matrix::CreateFromQuaternion(rotation) * translation * _modelTransforms[submodel.parent];
        }
        else {
            _modelTransforms[sm] = Matrix::CreateFromQuaternion(rotation) * translation;
        }
    }

    animation.elapsed = std::min(animation.elapsed + dt, animation.duration);
}

void PlayAnimation(const AnimationInstance& animation) {
    _animation = animation;
}

void UpdateAnimations(ModelID modelId, float dt) {
    if (auto entry = g_ModelCache.Get(modelId)) {
        AnimateModel(entry->model, _animation, dt);
    }
}

void DrawMeshPrepass(GraphicsContext& context, ModelID modelId) {
    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::modelPrepass);

    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto device = GetDevice();

    SetCommonShaderParmeters(cmdList);

    auto entry = g_ModelCache.Get(modelId);
    if (!entry) return;

    auto& model = entry->model;
    auto m = resources.meshPool->Get(entry->mesh);
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


void DrawMesh(GraphicsContext& context, ModelID modelId) {
    auto cmdList = context.GetCommandList();
    context.SetPipelineState(pipelines::model);

    auto& frameDescriptors = GetFrameDescriptors();
    auto& frameBuffer = GetFrameBuffer();

    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto device = GetDevice();

    SetCommonShaderParmeters(cmdList);

    auto entry = g_ModelCache.Get(modelId);
    if (!entry) return;

    auto& model = entry->model;
    auto m = resources.meshPool->Get(entry->mesh);
    if (!m) return;
    auto& mesh = *m;

    ASSERT(mesh.submeshes.size() == model.submodels.size());

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

            GetSpriteBatch().Add({
                .vertex = {
                    .position = constants.world.Translation(),
                    .color = submodel.glow,
                    .size = Vector2(submodel.glowSize * 0.5f, submodel.glowSize * 0.5f),
                },
                // todo: depth is only needed for alpha sprites, not additive
                .depth = Vector3::DistanceSquared(context.camera->Position, constants.world.Translation()),
                .texture = submesh.texture
            });

            continue;
        }


        // todo: not all facing submodels are sprites?
        if (HasFlag(submodel.flags, d3::SubmodelFlag::Facing)) {
            ASSERT((int)submesh.texture >= 0);

            auto size = submodel.max - submodel.min;

            GetSpriteBatch().Add({
                .vertex = {
                    .position = constants.world.Translation(),
                    .color = Color(1, 1, 1, 1),
                    .size = Vector2(std::max(size.x, size.z) * 0.5f, size.y * 0.5f), // todo: there is a more robust function for this in D3 / D3edit
                },
                // todo: depth is only needed for alpha sprites, not additive
                .depth = Vector3::DistanceSquared(context.camera->Position, constants.world.Translation()),
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

            context.SetPipelineState(pipelines::modelAlpha);
            cmdList->IASetIndexBuffer(&submesh.transparentIbv);
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

void Render(Camera& camera, RenderTarget& renderTarget, ModelID modelId) {
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

    // depth prepass
    context.SetRenderTarget(sizedResources.linearDepthBuffer, sizedResources.sceneDepthBuffer);
    context.ClearRenderTarget(sizedResources.linearDepthBuffer, nullptr);
    context.ClearDepth(sizedResources.sceneDepthBuffer);
    DrawMeshPrepass(context, modelId);

    // opaque pass
    context.SetRenderTarget(sizedResources.sceneColorBuffer, sizedResources.sceneDepthBuffer);
    Color background(0.05f, 0.05f, 0.05f);
    context.ClearRenderTarget(sizedResources.sceneColorBuffer, nullptr, &background);
    context.ClearDepth(sizedResources.sceneDepthBuffer);

    //if (Seq::inRange(resources.meshes, meshid))
    DrawMesh(context, modelId);

    // additive pass
    GetSpriteBatch().Upload();
    DrawSprites(context);

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

void RenderView(Camera& camera, ModelID modelid) {
    auto& renderTarget = sizedResources.backBuffers[_backBufferIndex];
    Render(camera, renderTarget, modelid);
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

}
