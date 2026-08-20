#pragma once
#include "Graphics/ShaderTypes.h"

namespace neon::gfx::shaders {

constexpr D3D12_ROOT_PARAMETER1 FrameConstantsParameter = {
    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
    .Descriptor = { .ShaderRegister = 0, .RegisterSpace = 1 }, // Frame constants, b0
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
};

constexpr D3D12_DESCRIPTOR_RANGE1 TextureTableRange = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    .NumDescriptors = UINT_MAX,
    .BaseShaderRegister = 1, // t0, space 1
    .RegisterSpace = 1,
    .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
};

constexpr D3D12_ROOT_PARAMETER1 TextureTableParameter = {
    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
    .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = &TextureTableRange },
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
};

constexpr D3D12_DESCRIPTOR_RANGE1 TextureInfoTable = {
    .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    .NumDescriptors = 1,
    .BaseShaderRegister = 0, // t0. Texture info table
    .RegisterSpace = 1,
    .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
};

constexpr D3D12_ROOT_PARAMETER1 TextureInfoTableParameter = {
    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
    .DescriptorTable = { .NumDescriptorRanges = 1, .pDescriptorRanges = &TextureInfoTable },
    //.Descriptor = {.ShaderRegister = 0 }, // Frame constants, b0
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
};

}
