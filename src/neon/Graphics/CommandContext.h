#pragma once

#include "Camera.h"
#include "neon-graphics.h"
#include "neon-types.h"
#include "neon-math.h"
#include "CommandQueue.h"
#include "GpuResource.h"
#include "ShaderTypes.h"

namespace neon::gfx {
    // Combined command list / allocator / queue for executing commands
    class CommandContext {
    protected:
        CommandQueue* _queue;
        ComPtr<ID3D12GraphicsCommandList> _cmdList;
        ComPtr<ID3D12CommandAllocator> _allocator;
        uint64 _nextFenceValue = 0;

    public:
        CommandContext(ID3D12Device* device, CommandQueue* queue, string_view name) : _queue(queue) {
            assert(device);
            assert(queue);
            ThrowIfFailed(device->CreateCommandAllocator(queue->GetType(), IID_PPV_ARGS(&_allocator)));
            ThrowIfFailed(_allocator->SetName(Widen(name).data()));

            ThrowIfFailed(device->CreateCommandList(1, queue->GetType(), _allocator.Get(), nullptr, IID_PPV_ARGS(&_cmdList)));
            ThrowIfFailed(_cmdList->SetName(Widen(name).data()));
            ThrowIfFailed(_cmdList->Close()); // Command lists start open
        }

        virtual ~CommandContext() = default;
        CommandContext(const CommandContext&) = delete;
        CommandContext(CommandContext&&) = delete;
        CommandContext& operator=(const CommandContext&) = delete;
        CommandContext& operator=(CommandContext&&) = delete;

        ID3D12GraphicsCommandList* GetCommandList() const { return _cmdList.Get(); }
        CommandQueue* GetCommandQueue() const { return _queue; }

        void ExecuteIndirect(ID3D12Device* device) {
            struct IndirectCommand : D3D12_DRAW_INDEXED_ARGUMENTS {
                // D3D12_DRAW_INDEXED_ARGUMENTS drawArguments; // 20 bytes
                // optional: root constants, CBV addresses, VB/IB bindings, etc.
            };

            D3D12_INDIRECT_ARGUMENT_DESC args[1] = {};
            args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            args[0].Constant.RootParameterIndex = 2;
            
            //D3D12_INDIRECT_ARGUMENT_TYPE Args[4] = {};
            //Args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            //Args[0].Constant.RootParameterIndex = 2;
            //Args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            //Args[1].Constant.RootParameterIndex = 6;
            //Args[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER;
            //Args[2].VertexBuffer.VBSlot = 3;
            //Args[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INSTANCED;

            D3D12_COMMAND_SIGNATURE_DESC sigDesc = {};
            sigDesc.ByteStride = sizeof(IndirectCommand); // must be 4-byte aligned
            sigDesc.NumArgumentDescs = 1;
            sigDesc.pArgumentDescs = args;

            ComPtr<ID3D12CommandSignature> commandSignature;

            ThrowIfFailed(device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&commandSignature)));

            //auto cmdList = GetCommandList();
            //cmdList->ExecuteIndirect(commandSignature.Get(), 2, pArgs, 0, nullptr, 0);

        }

        // Resets the context so new commands can be recorded
        void Reset() const {
            ThrowIfFailed(_allocator->Reset());
            ThrowIfFailed(_cmdList->Reset(_allocator.Get(), nullptr));
        }

        void Execute() {
            _nextFenceValue = _queue->Execute(_cmdList.Get());
        }

        // Blocks until command queue finishes execution
        void WaitForIdle() const {
            _queue->WaitForFence(_nextFenceValue);
        }

        // Waits on another queue
        //void InsertWaitForQueue(const CommandContext& /*other*/) const {
        //    ThrowIfFailed(_queue->Wait(other._fence.Get(), other._fenceValue - 1));
        //}
    };

    // Command context that supports rendering to a target
    class GraphicsContext : public CommandContext {
        uintptr_t _activeEffect = 0;

    public:
        GraphicsContext(ID3D12Device* device, CommandQueue* queue, string_view name) : CommandContext(device, queue, name) {}

        Camera* camera = nullptr;

        // Sets multiple render targets with a depth buffer. Used with shaders that write to multiple buffers.
        void SetRenderTargets(span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs, D3D12_CPU_DESCRIPTOR_HANDLE dsv) const {
            _cmdList->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), false, &dsv);
        }

        // Sets multiple render targets. Used with shaders that write to multiple buffers.
        void SetRenderTargets(span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs) const {
            _cmdList->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), false, nullptr);
        }

        void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv) const { SetRenderTargets({ &rtv, 1 }); }

        void SetRenderTarget(RenderTarget& renderTarget) const {
            renderTarget.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            auto rtv = renderTarget.GetRTV();
            SetRenderTargets({ &rtv, 1 });
        }

        void SetRenderTarget(RenderTarget& renderTarget, DepthBuffer& depthTarget) const {
            renderTarget.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            depthTarget.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
            auto rtv = renderTarget.GetRTV();
            auto dsv = depthTarget.GetDSV();
            SetRenderTargets({ &rtv, 1 }, dsv);
        }

        void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) const {
            ASSERT(rtv.ptr && dsv.ptr);
            SetRenderTargets({ &rtv, 1 }, dsv);
        }

        void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology) const {
            _cmdList->IASetPrimitiveTopology(topology);
        }

        void ClearRenderTarget(RenderTarget& target, const D3D12_RECT* rect = nullptr, const Color* color = nullptr) {
            target.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            _cmdList->ClearRenderTargetView(target.GetRTV(), color ? *color : target.GetClearColor(), (rect == nullptr) ? 0 : 1, rect);
            _activeEffect = 0;
        }

        void ClearRenderTarget(ColorBuffer& target, const D3D12_RECT* rect = nullptr, const Color* color = nullptr) {
            target.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            _cmdList->ClearRenderTargetView(target.GetRTV(), color ? *color : target.ClearColor, (rect == nullptr) ? 0 : 1, rect);
            _activeEffect = 0;
        }

        void ClearDepth(DepthBuffer& target, const D3D12_RECT* rect = nullptr, const float* depth = nullptr) {
            target.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
            _cmdList->ClearDepthStencilView(target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, depth ? *depth : target.ClearDepth, 0, (rect == nullptr) ? 0 : 1, rect);
            _activeEffect = 0;
        }

        void ClearStencil(DepthBuffer& target, uint8 value, const D3D12_RECT* rect = nullptr) {
            target.Transition(_cmdList.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
            _cmdList->ClearDepthStencilView(target.GetDSV(), D3D12_CLEAR_FLAG_STENCIL, target.ClearDepth, value, (rect == nullptr) ? 0 : 1, rect);
            _activeEffect = 0;
        }

        void SetScissor(const D3D12_RECT& scissor) const {
            _cmdList->RSSetScissorRects(1, &scissor);
        }

        void SetViewport(const uint2& size) const {
            D3D12_VIEWPORT viewport{};
            viewport.Width = (float)size.x;
            viewport.Height = (float)size.y;
            viewport.MinDepth = D3D12_MIN_DEPTH;
            viewport.MaxDepth = D3D12_MAX_DEPTH;
            _cmdList->RSSetViewports(1, &viewport);
        }

        void SetViewportAndScissor(const uint2& size) const {
            SetViewport(size);
            SetScissor({
                .left = 0,
                .top = 0,
                .right = (LONG)size.x,
                .bottom = (LONG)size.y
            });
        }

        void SetConstantsArray(uint rootIndex, uint numConstants, const void* data) const {
            _cmdList->SetGraphicsRoot32BitConstants(rootIndex, numConstants, data, 0);
        }

        void SetConstantBuffer(uint rootIndex, D3D12_GPU_VIRTUAL_ADDRESS cbv) const {
            _cmdList->SetGraphicsRootConstantBufferView(rootIndex, cbv);
        }

        //void SetDynamicConstantBuffer(uint rootIndex, size_t bufferSize, const void* data) {
        //    //_cmdList->SetGraphicsRootConstantBufferView(rootIndex, cbv);

        //    assert(data && IsAligned(data, 16));
        //    auto cb = m_CpuLinearAllocator.Allocate(bufferSize);
        //    //SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
        //    memcpy(cb.DataPtr, data, bufferSize);
        //    _cmdList->SetGraphicsRootConstantBufferView(rootIndex, cb.GpuAddress);
        //}

        bool SetPipelineState(PipelineInfo& pipeline, bool force = false) {
            if (_activeEffect == (uintptr_t)&pipeline && !force) return false;
            ASSERT(pipeline.pso); // forgot to compile shader
            _activeEffect = (uintptr_t)&pipeline;
            _cmdList->SetPipelineState(pipeline.pso);
            _cmdList->SetGraphicsRootSignature(pipeline.rootSignature);
            return true;
        }

    private:
        static constexpr bool IsAligned(auto value, size_t alignment) {
            return 0 == ((size_t)value & (alignment - 1));
        }
    };
}
