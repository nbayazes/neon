#pragma once

#include "ShaderTypes.h"
#include "neon-graphics.h"

namespace neon::gfx {

void InitShaderCompiler(ID3D12Device* device);

// Frees all shader compiler resources, including shaders
void FreeShaderCompiler();

// Compiles a graphics pipeline and caches it
void CompileGraphicsPipeline(PipelineInfo& info, bool ignoreCache);

// Frees any compiled shaders
void ClearShaderCache();

}
