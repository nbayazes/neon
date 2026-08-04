#pragma once
#include <D3D12MemAlloc.h>
#include <directxtk12/DirectXHelpers.h>
#include "Descriptor.h"
#include "neon-graphics.h"
#include "neon-math.h"
#include "neon.h"
#include "PlatformHelpers.h"

namespace neon::gfx {

class DescriptorHeap;

// Handle for a resource mapped to the GPU and CPU
struct MappedHandle {
    void* cpu = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
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

    void CopyRegionTo(ID3D12GraphicsCommandList* cmdList, GpuResource& dest, uint64 offset, uint64 destOffset, uint64 numBytes) const {
        dest.Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
        //Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdList->CopyBufferRegion(dest.Get(), destOffset, _resource.Get(), offset, numBytes);
    }

protected:
    virtual D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() { throw NotSupportedException(); }
    virtual D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc() { throw NotSupportedException(); }
    virtual D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc() { throw NotSupportedException(); }
    virtual D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc() { throw NotSupportedException(); }

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
    D3D12MA::VirtualBlock* _block = nullptr;
    uint _alignment = 0;

public:
    void Create(string_view name, uint64 size, uint alignment = 4);

    // Allocates a block of memory in the buffer
    D3D12MA::VIRTUAL_ALLOCATION_INFO Allocate(uint64 size) const {
        D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
        allocDesc.Size = size;
        allocDesc.Alignment = _alignment;

        D3D12MA::VirtualAllocation alloc;
        UINT64 allocOffset;
        ThrowIfFailed(_block->Allocate(&allocDesc, &alloc, &allocOffset));

        D3D12MA::VIRTUAL_ALLOCATION_INFO info{};
        _block->GetAllocationInfo(alloc, &info);
        return info;
    }

protected:
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

class FrameRingBuffer : public GpuResource {
    struct FrameAllocations {
        UINT64 fenceValue = 0; // queue fence value recorded on submit
        std::vector<D3D12MA::VirtualAllocation> allocs;
    };

    List<FrameAllocations> _frameAllocs;
    ubyte* _mappedPtr = nullptr;
    D3D12MA::VirtualBlock* _block = nullptr;
    uint _frames = 0;
public:
    void Create(string_view name, uint64 size, uint frames);

    // Copy data to the ring buffer and increments it internally.
    // Returns the offset.
    // Fence value is the value when the resource should be released.
    template <typename T>
    UINT64 Copy(uint64 frame, uint64 fenceValue, T& data, uint64 alignment = 4) {

        D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
        allocDesc.Size = sizeof(data);
        allocDesc.Alignment = alignment;

        D3D12MA::VirtualAllocation alloc;
        UINT64 offset;
        ThrowIfFailed(_block->Allocate(&allocDesc, &alloc, &offset));

        auto& slot = _frameAllocs[frame % _frames];
        slot.allocs.push_back(alloc);
        slot.fenceValue = fenceValue;

        memcpy(_mappedPtr + offset, &data, sizeof(data));
        return offset;
    }


    // Call at the end of each frame to free resources
    void Update(uint64 frame, uint64 fenceValue) {
        auto& slot = _frameAllocs[frame % _frames];

        if (fenceValue >= slot.fenceValue) {
            for (auto& alloc : slot.allocs) {
                _block->FreeAllocation(alloc);
            }

            slot.allocs.clear();
        }
    }
};

class GpuUploadBuffer : public GpuResource {
    bool _inUpdate = false;
    int64 _offset = 0;
    ubyte* _mappedPtr = nullptr;

public:
    void Create(string_view name, uint64 size);

    // Copy data immediately
    template <typename T>
    void ImmediateCopy(span<T>& data) {
        //constexpr D3D12_RANGE CPU_READ_NONE = {};
        void* mappedPtr;
        ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, &mappedPtr));

        memcpy(mappedPtr, data.data(), data.size());
        _resource->Unmap(0, nullptr);
    }

    template <typename T>
    void ImmediateCopy(T& data) {
        //constexpr D3D12_RANGE CPU_READ_NONE = {};
        void* mappedPtr;
        ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, &mappedPtr));

        memcpy(mappedPtr, &data, sizeof(data));
        _resource->Unmap(0, nullptr);
    }

    void BeginCopy() {
        ASSERT(_resource);
        if (_inUpdate) throw Exception("Already called Begin");

        ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedPtr));
        _inUpdate = true;
    }

    // Copies data into the buffer. Returns the offset.
    template <typename T>
    int64 Copy(span<T> src) {
        if (!_inUpdate)
            throw Exception("Must call Begin before Copy");

        if (_offset + src.size() > _desc.Width) {
            throw Exception("Out of space in upload buffer");
        }

        memcpy(_mappedPtr + _offset, src.data(), src.size());
        auto offset = _offset;
        _offset += src.size();
        return offset;
        //_buffer.insert(_buffer.end(), src.begin(), src.end());
    }

    void EndCopy() {
        _inUpdate = false;
        _offset = 0;
        _resource->Unmap(0, nullptr);
    }

    //void CopyTo(ID3D12GraphicsCommandList* cmdList, GpuResource& dest) {
    //    Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

    //    cmdList->CopyBufferRegion(dest.Get(), 0, _resource.Get(), 0, size);
    //    Transition(cmdList, D3D12_RESOURCE_STATE_COMMON);
    //}

    //void CopyTo(ID3D12GraphicsCommandList* cmdList, GpuResource& dest) {
    //    dest.Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
    //    Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
    //    cmdList->CopyResource(dest.Get(), _resource.Get());
    //}
};

class ByteAddressBuffer final : public GpuBuffer {
    uint _elementCount = 0; // elementCount / 4

protected:
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

protected:
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

protected:
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
};

class DepthBuffer : public PixelBuffer {
    DescriptorHandle _dsv, _roDescriptor;
    D3D12_DEPTH_STENCIL_VIEW_DESC _dsvDesc = {};

public:
    float ClearDepth = 1.0f;

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
        //_state = D3D12_RESOURCE_STATE_COMMON;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = format;
        clearValue.DepthStencil.Depth = ClearDepth;

        //D3D12MA::ALLOCATION_DESC allocDesc = {};
        //allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        //allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;

        //ThrowIfFailed(Render::Allocator->CreateResource(
        //    &allocDesc,
        //    &_desc,
        //    _state,
        //    &clearValue,
        //    _allocation.ReleaseAndGetAddressOf(),
        //    IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())
        //));

        CreateOnDefaultHeap(name, &clearValue);

        SetName(_resource, name);
    }

    void Clear(ID3D12GraphicsCommandList* commandList) {
        //assert(_state == D3D12_RESOURCE_STATE_DEPTH_WRITE);
        Transition(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        commandList->ClearDepthStencilView(_dsv.GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, ClearDepth, 0, 0, nullptr);
    }

protected:
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
};


// Generic render target
class RenderTarget : public PixelBuffer {
    Color _clearColor;

public:
    Color& GetClearColor() { return _clearColor; }

    // Creates a general purpose render target
    void Create(string_view name, UINT width, UINT height, DXGI_FORMAT format, const Color& clearColor = { 0, 0, 0 }, UINT samples = 1) {
        _clearColor = clearColor;

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
    void CreateBackBuffer(string_view name, IDXGISwapChain* swapChain, UINT buffer, const Color& clearColor = { 0, 0, 0 }) {
        _clearColor = clearColor;

        ThrowIfFailed(swapChain->GetBuffer(buffer, IID_PPV_ARGS(_resource.ReleaseAndGetAddressOf())));
        _desc = _resource->GetDesc();
        SetName(_resource, name);
    }

protected:
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
};

}
