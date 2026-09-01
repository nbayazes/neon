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

enum class CompilerStatus { Ok, Error };

struct CompilerEvent {
    CompilerStatus status;
    string file;
    string message;
};

struct CompilerResult {
    uint successes = 0;
    uint errors = 0;
    List<CompilerEvent> events;
};

void ClearCompilerEvents();

CompilerResult GetCompilerResult();

}
