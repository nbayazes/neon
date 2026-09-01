#pragma once

namespace neon::app {

void Update(float dt);
void Render();
void Init();
void TextureDebugWindow();
void OnMouseMoved(float x, float y, float xrel, float yrel);
void OnMouseWheel(float x, float y);
void OnMouseButtonDown(uint8_t button);
void OnMouseButtonUp(uint8_t button);

}
