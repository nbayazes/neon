#pragma once
#include "neon.h"
#include "neon-graphics.h"
#include "Descriptor.h"
#include "PlatformHelpers.h"
#include <D3D12MemAlloc.h>
#include <directxtk12/DirectXHelpers.h>
#include "neon-math.h"

namespace neon::gfx {
    class DescriptorHeap;

    // Handle for a resource mapped to the GPU and CPU
    struct MappedHandle {
        void* cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu{};
        uint64 offset = 0;
        ID3D12Resource* resource = nullptr;
    };

    // Intermediate resource for heap uploads.
    // The resource must exist until the command list finishes executing.
    class IntermediateResource {
    public:
        ComPtr<ID3D12Resource> resource;
        ComPtr<D3D12MA::Allocation> allocation;

        IntermediateResource() = default;
        IntermediateResource(const D3D12_RESOURCE_DESC& desc);
    };

    class GpuResource {
        friend class DescriptorHeap; // allow the heap to set handles
    protected:
        ComPtr<ID3D12Resource> _resource;
        ComPtr<D3D12MA::Allocation> _allocation;
        D3D12_RESOURCE_STATES _state = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_DESC _desc = {};
        string _name;
        DescriptorHandle _srv, _rtv, _uav, _dsv;

    public:
        GpuResource() = default;

        virtual ~GpuResource() = default;
        GpuResource(const GpuResource&) = delete;
        GpuResource(GpuResource&&) = default;
        GpuResource& operator=(const GpuResource&) = delete;
        GpuResource& operator=(GpuResource&&) = default;

        ID3D12Resource* Get() const { return _resource.Get(); }
        ID3D12Resource* operator->() { return _resource.Get(); }
        const ID3D12Resource* operator->() const { return _resource.Get(); }
        explicit operator bool() const { return _resource.Get() != nullptr; }
        void Release() { _resource.Reset(); }
        const D3D12_RESOURCE_DESC& Description() const { return _desc; }

        const D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return _srv.GetGpuHandle(); }
        const D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return _uav.GetGpuHandle(); }
        const D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return _rtv.GetCpuHandle(); }
        const D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return _dsv.GetCpuHandle(); }

        virtual D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() { throw NotSupportedException(); }
        virtual D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() { throw NotSupportedException(); }
        virtual D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() { throw NotSupportedException(); }
        virtual D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc() { throw NotSupportedException(); }

        string_view GetName() const { return _name; }

        // Returns the original state
        D3D12_RESOURCE_STATES Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES state, bool force = false) {
            if (_state == state && !force) return _state;
            DirectX::TransitionResource(cmdList, _resource.Get(), _state, state);

            if (state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.UAV.pResource = _resource.Get();
                cmdList->ResourceBarrier(1, &barrier);
            }

            auto originalState = _state;
            _state = state;
            return originalState;
        }

        void CopyTo(ID3D12GraphicsCommandList* cmdList, GpuResource& dest) {
            dest.Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
            Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
            cmdList->CopyResource(dest.Get(), _resource.Get());
        }

        void CopyFrom(ID3D12GraphicsCommandList* cmdList, GpuResource& src) {
            Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
            src.Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
            cmdList->CopyResource(Get(), src._resource.Get());
        }

    protected:
        void CreateOnUploadHeap(string_view name, const D3D12_CLEAR_VALUE* clearValue = nullptr, bool forceComitted = false) {
            Create(D3D12_HEAP_TYPE_UPLOAD, name, clearValue, forceComitted);
        }

        void CreateOnDefaultHeap(string_view name, const D3D12_CLEAR_VALUE* clearValue = nullptr, bool forceComitted = false) {
            Create(D3D12_HEAP_TYPE_DEFAULT, name, clearValue, forceComitted);
        }

        void Create(D3D12_HEAP_TYPE heapType, string_view name, const D3D12_CLEAR_VALUE* clearValue, bool forceComitted);

        //[[nodiscard]] IntermediateResource CreateIntermediate();
    };

    // General purpose buffer
    class GpuBuffer : public GpuResource {
        uint _elementCount = 0;

    public:
        //void CreateGenericBuffer(string_view name, uint32 elementSize, uint32 elementCount) {
        //    _desc = CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount);
        //    _state = D3D12_RESOURCE_STATE_GENERIC_READ;

        //    _srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        //    _srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        //    _srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //    _srvDesc.Buffer.NumElements = elementCount;
        //    _srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //    D3D12MA::ALLOCATION_DESC allocDesc = {};
        //    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        //    ThrowIfFailed(Render::Allocator->CreateResource(
        //        &allocDesc,
        //        &_desc,
        //        _state,
        //        nullptr,
        //        _allocation.ReleaseAndGetAddressOf(),
        //        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
        //    ));

        //    if (!_srv) _srv = Render::Descriptors->reserved.Allocate();
        //    Render::Device->CreateShaderResourceView(Get(), &_srvDesc, _srv.GetCpuHandle());
        //    SetName(name);
        //}

        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Buffer.NumElements = _elementCount;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            return desc;
        }
    };

    class ByteAddressBuffer final : public GpuBuffer {
        uint _elementCount = 0; // elementCount / 4

    public:
        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Buffer.NumElements = _elementCount;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            return desc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() override {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.Buffer.NumElements = _elementCount;
            desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            return desc;
        }

        //void Create(string_view name, uint32 elementSize, uint32 elementCount) {
        //    _desc = CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        //    _srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        //    _srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        //    _srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //    _srvDesc.Buffer.NumElements = elementCount / 4;
        //    _srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

        //    D3D12MA::ALLOCATION_DESC allocDesc = {};
        //    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        //    ThrowIfFailed(Render::Allocator->CreateResource(
        //        &allocDesc,
        //        &_desc,
        //        D3D12_RESOURCE_STATE_COMMON,
        //        nullptr,
        //        _allocation.ReleaseAndGetAddressOf(),
        //        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
        //    ));

        //    //if (!_srv) _srv = Render::Heaps->Reserved.Allocate();

        //    _uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        //    _uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        //    _uavDesc.Buffer.NumElements = elementCount / 4;
        //    _uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

        //    //if (!_uav) _uav = Render::Heaps->Reserved.Allocate();
        //    SetName(name);

        //    //D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

        //    //D3D12_HEAP_PROPERTIES HeapProps;
        //    //HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        //    //HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        //    //HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        //    //HeapProps.CreationNodeMask = 1;
        //    //HeapProps.VisibleNodeMask = 1;


        //    //if (initialData)
        //    //    CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);

        //    //Render::Device->CreateUnorderedAccessView(Get(), nullptr, &uavDesc, _uav.GetCpuHandle());
        //    //_resource->SetName(name.data());
        //}
    };

    class StructuredBuffer final : public GpuBuffer {
        ByteAddressBuffer _counterBuffer;
        uint32 _elementSize = 0;
        uint32 _elementCount = 0;

    public:
        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Buffer.NumElements = _elementCount;
            desc.Buffer.StructureByteStride = _elementSize;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            return desc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() override {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.CounterOffsetInBytes = 0;
            desc.Buffer.NumElements = _elementCount;
            desc.Buffer.StructureByteStride = _elementSize;
            desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            return desc;
        }


        //void Create(string_view name, uint32 elementSize, uint32 elementCount) {
        //    _desc = CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        //    if (!_srv) _srv = Render::Descriptors->reserved.Allocate();

        //    D3D12MA::ALLOCATION_DESC allocDesc = {};
        //    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        //    ThrowIfFailed(Render::Allocator->CreateResource(
        //        &allocDesc,
        //        &_desc,
        //        D3D12_RESOURCE_STATE_COMMON,
        //        nullptr,
        //        _allocation.ReleaseAndGetAddressOf(),
        //        IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
        //    ));

        //    SetName(name);

        //    //_counterBuffer.Create("StructuredBuffer::Counter", 1, 4);
        //}
    };

    class PixelBuffer : public GpuResource {
    public:
        uint64 GetWidth() const { return _desc.Width; }
        uint64 GetHeight() const { return _desc.Height; }
        uint64 GetPitch() const { return _desc.Width * sizeof(uint32); }
        uint2 GetSize() const { return { (uint32)_desc.Width, (uint32)_desc.Height }; }

        DXGI_FORMAT GetFormat() const { return _desc.Format; }

        bool IsMultisampled() const { return _desc.SampleDesc.Count > 1; }

        // Copies a MSAA source into a non-sampled buffer
        void ResolveFromMultisample(ID3D12GraphicsCommandList* commandList, PixelBuffer& src) {
            if (!src.IsMultisampled())
                throw std::exception("Source must be multisampled");

            src.Transition(commandList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            Transition(commandList, D3D12_RESOURCE_STATE_RESOLVE_DEST);

            if (src._desc.DepthOrArraySize > 1) {
                for (int i = 0; i < 6; i++) {
                    commandList->ResolveSubresource(Get(), i, src.Get(), i, src._desc.Format);
                }
            }
            else {
                commandList->ResolveSubresource(Get(), 0, src.Get(), 0, src._desc.Format);
            }

            src.Transition(commandList, D3D12_RESOURCE_STATE_COMMON);
        }
    };

    // Intermediate render target
    class ColorBuffer : public PixelBuffer {
    public:
        Color ClearColor = { 0, 0, 0, 1 };

        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.ViewDimension = _desc.SampleDesc.Count == 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DMS;
            desc.Texture2D.MipLevels = 1;
            desc.Texture2D.MostDetailedMip = 0;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            return desc;
        }

        D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() override {
            D3D12_RENDER_TARGET_VIEW_DESC desc{};
            desc.Format = _desc.Format;
            desc.ViewDimension = _desc.SampleDesc.Count == 1 ? D3D12_RTV_DIMENSION_TEXTURE2D : D3D12_RTV_DIMENSION_TEXTURE2DMS;
            return desc;
        }

        void Create(string_view name, uint width, uint height, DXGI_FORMAT format, int samples = 1) {
            _desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, samples);
            _desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if (samples == 1)
                _desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = format;
            memcpy(clearValue.Color, ClearColor, sizeof(ClearColor));

            CreateOnDefaultHeap(name, &clearValue);

            //_rtvDesc.Format = format;
            //_rtvDesc.ViewDimension = samples == 1 ? D3D12_RTV_DIMENSION_TEXTURE2D : D3D12_RTV_DIMENSION_TEXTURE2DMS;

            //_srvDesc.ViewDimension = samples == 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DMS;
            //_srvDesc.Texture2D.MipLevels = 1;
            //_srvDesc.Texture2D.MostDetailedMip = 0;
            //_srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            _state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    };

    class DepthBuffer : public PixelBuffer {
        DescriptorHandle _dsv, _roDescriptor;
        D3D12_DEPTH_STENCIL_VIEW_DESC _dsvDesc = {};

    public:
        float ClearDepth = 1.0f;

        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.ViewDimension = _desc.SampleDesc.Count == 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DMS;
            desc.Texture2D.MipLevels = 1;
            desc.Texture2D.MostDetailedMip = 0;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Format = _desc.Format;
            return desc;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc() override {
            D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
            desc.Format = _desc.Format;
            desc.ViewDimension = _desc.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
            return desc;
        }


        void Create(string_view name, uint width, uint height, DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT, uint samples = 1) {
            _desc = CD3DX12_RESOURCE_DESC::Tex2D(
                format,
                width,
                height,
                1, // This depth stencil view has only one texture.
                1, // Use a single mipmap level.
                samples
            );
            _desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            _state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = format;
            clearValue.DepthStencil.Depth = ClearDepth;

            D3D12MA::ALLOCATION_DESC allocDesc = {};
            allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            SetName(_resource, name);

            //ThrowIfFailed(Render::Allocator->CreateResource(
            //    &allocDesc,
            //    &_desc,
            //    _state,
            //    &clearValue,
            //    _allocation.ReleaseAndGetAddressOf(),
            //    IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
            //));
        }

        void Clear(ID3D12GraphicsCommandList* commandList) {
            //assert(_state == D3D12_RESOURCE_STATE_DEPTH_WRITE);
            Transition(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            commandList->ClearDepthStencilView(_dsv.GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, ClearDepth, 0, 0, nullptr);
        }
    };


    // Generic render target
    class RenderTarget : public PixelBuffer {
    public:
        Color ClearColor;

        D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() override {
            return {
                .Format = _desc.Format,
                .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
            };
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() override {
            return {
                .ViewDimension = _desc.SampleDesc.Count == 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DMS,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = {
                    .MostDetailedMip = 0,
                    .MipLevels = 1,
                },
            };
        }

        // Creates a general purpose render target
        void Create(string_view name, UINT width, UINT height, DXGI_FORMAT format, const Color& clearColor = { 0, 0, 0 }, UINT samples = 1) {
            ClearColor = clearColor;

            _desc = CD3DX12_RESOURCE_DESC::Tex2D(
                format,
                width,
                height,
                1, // Render targets only have one texture
                1, // Single mipmap level
                samples
            );

            _desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if (samples == 1)
                _desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // MSAA doesn't allow UAVs

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = format;
            memcpy(clearValue.Color, clearColor, sizeof(clearColor));
            CreateOnDefaultHeap(name, &clearValue);
        }

        // Creates a render target for a swap chain
        void CreateBackBuffer(string_view name, IDXGISwapChain* swapChain, UINT buffer) {
            ThrowIfFailed(swapChain->GetBuffer(buffer, IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())));
            _desc = _resource->GetDesc();
            SetName(_resource, name);
        }
    };
}
