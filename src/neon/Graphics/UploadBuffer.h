#pragma once

#include "Descriptor.h"
#include "DeviceResources.h"
#include "neon-graphics.h"
#include "neon-types.h"

namespace neon::gfx {
    constexpr D3D12_RANGE CPU_READ_NONE = {};
    //inline const D3D12_RANGE* CPU_READ_ALL = nullptr;

    // Creates a buffer upload heap
    void CreateUploadHeap(ComPtr<ID3D12Resource>& resource, uint64 bufferSize);

    // Resizable buffer that uses the upload heap every frame.
    // intended for use with small dynamic buffers
    template <class T>
    class UploadBuffer {
        ComPtr<ID3D12Resource> _resource;
        bool _inUpdate = false;
        T* _mappedData = nullptr;
        size_t _gpuCapacity = 0, _requestedCapacity, _gpuElements = 0;
        List<T> _buffer;
        DescriptorHandle _srv, _uav;
        string _name;

    public:
        UploadBuffer(size_t capacity, string_view name) : _requestedCapacity(capacity), _name(name) {
            _buffer.reserve(capacity);
            _gpuCapacity = _requestedCapacity;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return _resource->GetGPUVirtualAddress(); }
        uint GetSizeInBytes() const { return (uint)(sizeof(T) * _gpuCapacity); }
        uint GetElementCount() const { return (uint)_gpuElements; }
        static uint GetStride() { return sizeof(T); }

        D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return _srv.GetGpuHandle(); }
        D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return _uav.GetGpuHandle(); }

        ID3D12Resource* Get() const { return _resource.Get(); }

        //void CreateShaderResourceView() {
        //    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        //    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        //    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        //    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //    srvDesc.Buffer.NumElements = (UINT)_buffer.size();
        //    srvDesc.Buffer.StructureByteStride = GetStride();
        //    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //    if (!_srv) _srv = Render::Descriptors->reserved.Allocate();
        //    Render::Device->CreateShaderResourceView(_resource.Get(), &srvDesc, _srv.GetCpuHandle());
        //}

        //void CreateUnorderedAccessView() {
        //    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        //    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        //    desc.Format = DXGI_FORMAT_UNKNOWN;
        //    desc.Buffer.CounterOffsetInBytes = 0;
        //    desc.Buffer.NumElements = _buffer.size();
        //    desc.Buffer.StructureByteStride = Stride;
        //    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        //    if (!_uav) _uav = Render::Heaps->Reserved.Allocate();
        //    Render::Device->CreateUnorderedAccessView(_resource.Get(), nullptr, &desc, _uav.GetCpuHandle());
        //}

        void Begin() {
            if (_inUpdate) throw Exception("Already called Begin");
            _inUpdate = true;

            bool shouldGrow = _requestedCapacity > _gpuCapacity;
            if (!_resource || shouldGrow) {
                if (shouldGrow)
                    _gpuCapacity = size_t(_requestedCapacity * 1.5);
                CreateUploadHeap(_resource, _gpuCapacity * sizeof(T));
                std::ignore = _resource->SetName(Widen(_name).c_str());

                //if (_mapped) _resource->Unmap(0, &CPU_READ_NONE);
                // leave the buffer mapped
                ThrowIfFailed(_resource->Map(0, &CPU_READ_NONE, (void**)&_mappedData));
            }

            _buffer.clear();
        }

        void Copy(span<T> src) {
            if (!_inUpdate)
                throw Exception("Must call Begin before Copy");

            if (_buffer.size() + src.size() > _gpuCapacity) {
                _requestedCapacity = _buffer.size() + src.size();
                return;
            }

            _buffer.insert(_buffer.end(), src.begin(), src.end());
        }

        void Copy(T& src) {
            if (!_inUpdate)
                throw Exception("Must call Begin before Copy");

            if (_buffer.size() + 1 > _gpuCapacity) {
                _requestedCapacity = _buffer.size() + 1;
                return;
            }

            //_buffer.insert(_buffer.end(), src.begin(), src.end());
            _buffer.insert(_buffer.end(), src);
        }


        bool End() {
            if (!_inUpdate)
                throw Exception("Must call Begin before End");

            _inUpdate = false;

            // copy to GPU
            memcpy(_mappedData, _buffer.data(), _buffer.size() * sizeof(T));
            _gpuElements = _buffer.size();
            return true;
        }
    };
}
