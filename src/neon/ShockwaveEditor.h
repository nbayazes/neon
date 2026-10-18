#pragma once
#include "Editor.h"
#include "TextureRegistry.h"
#include "imgui.h"
#include "VisualEffect.h"
#include "Scene.h"

namespace neon {

// Debug window with sliders for every property of a gfx::ShockwaveEffect.
// Draw() returns true when a value changed, apply it with ApplyTo.
class ShockwaveEditor {
    gfx::ShockwaveEffect _effect{};
    bool _visible = false;
    bool _changed = false;
    float _labelColumnWidth = 0;

public:
    gfx::ShockwaveEffect& Effect() noexcept { return _effect; }
    const gfx::ShockwaveEffect& Effect() const noexcept { return _effect; }

    void SetEffect(const gfx::ShockwaveEffect& effect) noexcept { _effect = effect; }

    bool Visible() const noexcept { return _visible; }
    void Show(bool visible = true) noexcept { _visible = visible; }

    void Toggle() noexcept { _visible = !_visible; }

    void ApplyTo(gfx::ShockwaveEffect& effect) const {
        effect = _effect;
    }

    // Draws the window. Returns true when a property changed since the last draw.
    bool Draw(Scene& scene, const char* title = "Shockwave Editor") {
        _changed = false;
        if (!_visible) return false;

        if (ImGui::Begin(title, &_visible)) {
            if (ImGui::Button("Reset")) {
                _effect = {};
                _changed = true;
            }

            ImGui::SeparatorText("Appearance");
            _changed |= imgui::SliderRow("Duration", _effect.duration, 0.0f, 2.0f, _labelColumnWidth);
            _changed |= imgui::SliderRow("Radius", _effect.radius, 0.0f, 20.0f, _labelColumnWidth);
            _changed |= imgui::SliderRow("Count", _effect.count, 1, 10, _labelColumnWidth);
            _changed |= imgui::TextureRow("Texture", g_TextureRegistry, _effect.texture, _labelColumnWidth);
            _changed |= imgui::CheckboxRow("Random Rotation", _effect.randomRotation, _labelColumnWidth);
            _changed |= imgui::CheckboxRow("Face Camera", _effect.faceCamera, _labelColumnWidth);
            _changed |= imgui::ColorRow("Color", _effect.color, _labelColumnWidth);

            if (ImGui::Button("Play")) {
                for (uint i = 0; i < _effect.count; ++i) {
                    bool found = false;
                    auto effect = _effect;

                    if (effect.randomRotation) {
                        effect.rotation = VectorToRotation(RandomDirection());
                        effect.cameraRotation = RandomFloat(0, DirectX::XM_2PI);
                    }

                    for (auto& shockwave : scene.shockwaves) {
                        if (shockwave.elapsed > shockwave.duration) {
                            shockwave = effect;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        scene.shockwaves.push_back(effect);
                }
            }
        }

        ImGui::End();
        return _changed;
    }
};

}
