#pragma once

#include "Graphics/ShaderCompiler.h"
#include "compose.h"
#include "imgui.h"
#include "rmlui.h"

namespace neon::gfx::shaders {
inline void Compile() {
    CompileGraphicsPipeline(pipelines::imgui);
    CompileGraphicsPipeline(pipelines::rmlui);
    CompileGraphicsPipeline(pipelines::compose);
}
}
