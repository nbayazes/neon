#include "pch.h"
#include "CommandQueue.h"

#include <algorithm>
#include "neon.h"

namespace neon::gfx {
    CommandQueue::CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, string_view name): _type(type) {
        ASSERT(device);
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = type;

        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_queue)));
        ThrowIfFailed(_queue->SetName(Widen(name).c_str()));

        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
        ThrowIfFailed(_fence->SetName(Widen(name).c_str()));

        //ThrowIfFailed(_fence->Signal((uint64)type << 56));

        //_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        //if (!_fenceEvent.IsValid())
        //    throw std::exception("CreateEvent");
    }

    uint64 CommandQueue::Execute(ID3D12GraphicsCommandList* cmdList) {
        ThrowIfFailed(cmdList->Close());
        ID3D12CommandList* ppCommandLists[] = { cmdList };
        _queue->ExecuteCommandLists(1, ppCommandLists);
        return IncrementFence();
    }

    bool CommandQueue::IsFenceComplete(uint64 value) {
        // Avoid querying the fence value by testing against the last one seen.
        // The max() is to protect against an unlikely race condition that could cause the last
        // completed fence value to regress.
        if (value > _lastCompletedValue)
            _lastCompletedValue = std::max(_lastCompletedValue, _fence->GetCompletedValue());

        return value <= _lastCompletedValue;
    }

    void CommandQueue::WaitForFence(uint64 value) {
        if (IsFenceComplete(value))
            return;

        std::scoped_lock lock(_eventMutex);
        ThrowIfFailed(_fence->SetEventOnCompletion(value, nullptr)); // null event causes infinite wait here
        //ThrowIfFailed(_fence->SetEventOnCompletion(value, _fenceEvent.Get()));
        //WaitForSingleObject(_fenceEvent.Get(), INFINITE);
        _lastCompletedValue = value;
    }

    uint64 CommandQueue::IncrementFence() {
        std::scoped_lock lock(_eventMutex);
        ThrowIfFailed(_queue->Signal(_fence.Get(), _nextFenceValue));
        return _nextFenceValue++;
    }
}
