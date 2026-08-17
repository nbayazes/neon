#pragma once

#include "Graphics/ShaderCompiler.h"
#include "compose.h"
#include "imgui.h"
#include "Model.h"
#include "rmlui.h"

namespace neon::gfx::shaders {

inline void Compile(bool ignoreCache = false) {
    CompileGraphicsPipeline(pipelines::imgui, ignoreCache);
    CompileGraphicsPipeline(pipelines::rmlui, ignoreCache);
    CompileGraphicsPipeline(pipelines::compose, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelAdditive, ignoreCache);
}

}
