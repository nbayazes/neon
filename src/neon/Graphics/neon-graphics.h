#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <directxtk12/CommonStates.h>
#include "PlatformHelpers.h"
#include "Widechar.h"

namespace neon::gfx {
    using Microsoft::WRL::ComPtr;

    void SetName(auto& comPtr, std::string_view name) {
        comPtr->SetName(Widen(name).c_str());
    }

    constexpr D3D12_RANGE CPU_READ_NONE = {};
}
