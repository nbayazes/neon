#pragma once

//#include <CodeAnalysis/Warnings.h>
//#pragma warning(push)
//#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

struct SDL_Window;

namespace ImGui {
// Copy of Selectable() with a border when selected
//bool ToggleButton(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size_arg, float borderSize = 1);

void SeparatorVertical();
}

namespace neon::imgui {
void Initialize(SDL_Window* window, float fontSize = 24);

void InitializeGraphics(UINT backBufferCount);

void FreeGraphics();

// Draws previously recorded commands
void Draw();

void Shutdown();

}

//#pragma warning(pop)
