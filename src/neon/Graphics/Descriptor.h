#pragma once

#include "neon.h"
#include "neon-graphics.h"

namespace neon::gfx {
    // Combined CPU and GPU descriptor handle
    struct DescriptorHandle {
        DescriptorHandle() = default;

        DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu, uint32 descriptorSize)
            : _cpuHandle(cpu), _gpuHandle(gpu), _descriptorSize(descriptorSize) {}

        bool IsShaderVisible() const { return _gpuHandle.ptr != 0; }
        explicit operator bool() const { return _cpuHandle.ptr != 0; }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return _cpuHandle; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return _gpuHandle; }

        DescriptorHandle Offset(int index) const {
            auto copy = *this;
            if (copy._cpuHandle.ptr) copy._cpuHandle.Offset(index, _descriptorSize);
            if (copy._gpuHandle.ptr) copy._gpuHandle.Offset(index, _descriptorSize);
            return copy;
        }

        uint DescriptorSize() const { return _descriptorSize; }

    private:
        CD3DX12_CPU_DESCRIPTOR_HANDLE _cpuHandle{};
        CD3DX12_GPU_DESCRIPTOR_HANDLE _gpuHandle{};
        uint32 _descriptorSize = 0;
    };

}