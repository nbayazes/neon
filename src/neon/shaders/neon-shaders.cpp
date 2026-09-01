#include "pch.h"
#include "neon-shaders.h"
#include "Graphics/ShaderCompiler.h"
#include "compose.h"
#include "imgui.h"
#include "Model.h"
#include "ModelPrepass.h"
#include "Sprite.h"
#include "rmlui.h"

namespace neon::app {
void ShowCompilerOutput();
}

namespace neon::gfx::shaders {

void Compile(bool ignoreCache) {
    ClearCompilerEvents();
    // todo: invalidate cache command. 
    // ignoreCache incorrectly recompiles the same shader multiple times when it is shared across pipelines
    CompileGraphicsPipeline(pipelines::imgui, ignoreCache);
    CompileGraphicsPipeline(pipelines::rmlui, ignoreCache);
    CompileGraphicsPipeline(pipelines::compose, ignoreCache);
    CompileGraphicsPipeline(pipelines::model, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelAdditive, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelAlpha, ignoreCache);
    CompileGraphicsPipeline(pipelines::modelPrepass, ignoreCache);
    CompileGraphicsPipeline(pipelines::sprite, ignoreCache);
    CompileGraphicsPipeline(pipelines::spriteAdditive, ignoreCache);

    app::ShowCompilerOutput();
}

}
