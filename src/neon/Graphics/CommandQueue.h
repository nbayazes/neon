#pragma once

#define NOMINMAX

#include <mutex>

#include "neon-graphics.h"
#include "neon-types.h"


namespace neon::gfx {
    // Wrapper for ID3D12CommandQueue that supports waiting
    class CommandQueue {
        D3D12_COMMAND_LIST_TYPE _type;
        ComPtr<ID3D12CommandQueue> _queue;
        ComPtr<ID3D12Fence> _fence;
        //Microsoft::WRL::Wrappers::Event _fenceEvent; // todo: rewrite this? the corewrapper.h is quite large
        uint64 _nextFenceValue = 1, _lastCompletedValue = 0;
        std::mutex _eventMutex;

    public:
        CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, string_view name);

        void WaitForIdle() {
            WaitForFence(IncrementFence());
        }

        uint64 Execute(ID3D12GraphicsCommandList* cmdList);

        D3D12_COMMAND_LIST_TYPE GetType() const { return _type; }

        ID3D12CommandQueue* Get() const { return _queue.Get(); }

        uint64 GetCompletedValue() const { return _lastCompletedValue; }
        uint64 GetNextValue() const { return _nextFenceValue; }
        void WaitForFence(uint64 value);

    private:
        bool IsFenceComplete(uint64 value);


        // Signal the next fence value (with the GPU)
        uint64 IncrementFence();
    };

}
