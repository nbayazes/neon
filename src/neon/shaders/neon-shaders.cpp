#include "pch.h"
#include "neon-shaders.h"
#include "Graphics/ShaderCompiler.h"
#include "compose.h"
#include "imgui.h"
#include "Model.h"
#include "ModelPrepass.h"
#include "Sprite.h"
#include "rmlui.h"

namespace neon::gfx::shaders {

void Compile(bool ignoreCache) {
    CompileGraphicsPipeline(pipelines::imgui, ignoreCache);
    CompileGraphicsPipeline(pipelines::rmlui, ignoreCache);
    CompileGraphicsPipeline(pipelines::compose, ignoreCache);
    CompileGraphicsPipeline(pipelines::model, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelAdditive, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelAlpha, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelPrepass, ignoreCache);
    CompileGraphicsPipeline(pipelines::sprite, ignoreCache);
    CompileGraphicsPipeline(pipelines::spriteAdditive, ignoreCache);
}

}
