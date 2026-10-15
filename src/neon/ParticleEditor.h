#pragma once
#include "Editor.h"
#include "Graphics/ParticleSystem.h"
#include "TextureRegistry.h"
#include "imgui.h"

namespace neon {

// Debug window with sliders for every property of a gfx::ParticleSystemInfo.
// Draw() returns true when a value changed, apply it with gfx::ParticleSystem::Initialize.
class ParticleEditor {
    gfx::ParticleSystemInfo _info{};
    bool _visible = false;
    bool _changed = false;
    Vector3 _forward;

public:
    gfx::ParticleSystemInfo& Info() noexcept { return _info; }
    const gfx::ParticleSystemInfo& Info() const noexcept { return _info; }

    void SetInfo(const gfx::ParticleSystemInfo& info) noexcept { _info = info; }

    bool Visible() const noexcept { return _visible; }
    void Show(bool visible = true) noexcept { _visible = visible; }

    void Toggle() noexcept { _visible = !_visible; }

    void ApplyTo(gfx::ParticleSystem& system) {
        _info.SetForwardDirection(_forward);
        system.info = _info;
    }

    // Draws the window. Returns true when a property changed since the last draw.
    bool Draw(const char* title = "Particle Editor") {
        _changed = false;
        if (!_visible) return false;

        if (ImGui::Begin(title, &_visible)) {
            if (ImGui::Button("Reset")) {
                _info = {};
                _changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Direction of (0, 0, 0) spawns in a random direction");

            ImGui::SeparatorText("Transform");
            _changed |= imgui::SliderRow("Position", _info.position, -10.0f, 10.0f);
            _changed |= imgui::SliderRow("Direction", _forward, -1.0f, 1.0f);
            _changed |= imgui::SliderRow("Cone Radius", _info.coneRadius, 0.0f, 1.0f);
            _changed |= imgui::SliderRow("Spawn Radius", _info.spawnRadius, 0.0f, 10.0f);

            ImGui::SeparatorText("Appearance");
            _changed |= imgui::SliderRow("Middle offset", _info.midOffset, 0, 1);
            _changed |= imgui::ColorRow("Start Color", _info.startColor);
            _changed |= imgui::ColorRow("Mid Color", _info.midColor);
            _changed |= imgui::ColorRow("End Color", _info.endColor);
            _changed |= imgui::SliderRow("Start Size", _info.startSize, 0.0f, 10.0f);
            _changed |= imgui::SliderRow("Mid Size", _info.midSize, 0.0f, 10.0f);
            _changed |= imgui::SliderRow("End Size", _info.endSize, 0.0f, 10.0f);
            _changed |= imgui::TextureRow("Texture", g_TextureRegistry, _info.texture);
            _changed |= imgui::BlendRow("Blend mode", _info.blendMode);
            _changed |= imgui::CheckboxRow("Stretch Animation", _info.stretchAnimation);
            _changed |= imgui::CheckboxRow("Random Rotation", _info.randomRotation);
            _changed |= imgui::RangeRows("Rotation speed", _info.rotationSpeed, -5.0f, 5.0f);

            ImGui::SeparatorText("Motion");
            _changed |= imgui::SliderRow("Gravity", _info.gravity, -20.0f, 20.0f);
            _changed |= imgui::SliderRow("Wind", _info.wind, -20.0f, 20.0f);
            _changed |= imgui::SliderRow("Drag", _info.drag, 0.0f, 1.0f);
            _changed |= imgui::RangeRows("Velocity", _info.velocity, 0.0f, 100.0f);
            _changed |= imgui::SliderRow("Restitution", _info.restitution, 0.0f, 1.0f);
            _changed |= imgui::CheckboxRow("World Collision", _info.useCollision);

            ImGui::SeparatorText("Emission");
            _changed |= imgui::RangeRows("Count", _info.count, 1, 20);
            _changed |= imgui::RangeRows("Duration", _info.duration, 0.0f, 5.0f);
            _changed |= imgui::RangeRows("Interval", _info.interval, 0.0f, 1.0f);
        }

        ImGui::End();
        return _changed;
    }
};

}
