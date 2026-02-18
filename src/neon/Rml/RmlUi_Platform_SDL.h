#pragma once

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

#include <SDL3/SDL.h>

class SystemInterface_SDL : public Rml::SystemInterface {
public:
	SystemInterface_SDL(SDL_Window* in_window);
	~SystemInterface_SDL() override;

	// -- Inherited from Rml::SystemInterface  --

	double GetElapsedTime() override;

	void SetMouseCursor(const Rml::String& cursor_name) override;

	void SetClipboardText(const Rml::String& text) override;
	void GetClipboardText(Rml::String& text) override;

	void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
	void DeactivateKeyboard() override;

    SystemInterface_SDL(const SystemInterface_SDL& other) = delete;
    SystemInterface_SDL(SystemInterface_SDL&& other) noexcept = delete;
    SystemInterface_SDL& operator=(const SystemInterface_SDL& other) = delete;
    SystemInterface_SDL& operator=(SystemInterface_SDL&& other) noexcept = delete;

private:
	SDL_Window* window = nullptr;

	SDL_Cursor* cursor_default = nullptr;
	SDL_Cursor* cursor_move = nullptr;
	SDL_Cursor* cursor_pointer = nullptr;
	SDL_Cursor* cursor_resize = nullptr;
	SDL_Cursor* cursor_cross = nullptr;
	SDL_Cursor* cursor_text = nullptr;
	SDL_Cursor* cursor_unavailable = nullptr;
};

namespace RmlSDL {

	// Applies input on the context based on the given SDL event.
	// 
	// Note (SDL3 + SDL_Renderer): When using SDL_SetRenderLogicalPresentation(), SDL_Renderer operates in render
	// coordinates (logical coordinates). Therefore, before passing an SDL_Event to InputEventHandler, input event
	// coordinates (mouse/touch/etc.) should be converted to render coordinates, e.g.
	// SDL_ConvertEventToRenderCoordinates(renderer, &ev).
	// @return True if the event is still propagating, false if it was handled by the context.
	bool InputEventHandler(Rml::Context* context, SDL_Window* window, SDL_Event& ev);

	// Converts the SDL key to RmlUi key.
	Rml::Input::KeyIdentifier ConvertKey(int sdl_key);

	// Converts the SDL mouse button to RmlUi mouse button.
	int ConvertMouseButton(int sdl_mouse_button);

	// Returns the active RmlUi key modifier state.
	int GetKeyModifierState();

} // namespace RmlSDL