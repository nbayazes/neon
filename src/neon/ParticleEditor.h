#pragma once
#include "Graphics/ParticleSystem.h"
#include "TextureRegistry.h"
#include "imgui.h"

namespace neon {

namespace imgui {

    inline void RowLabel(const char* label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    inline bool SliderRow(const char* label, float& value, float min, float max) {
        ImGui::PushID(label);
        RowLabel(label);
        bool changed = ImGui::SliderFloat("##value", &value, min, max);
        ImGui::PopID();
        return changed;
    }

    inline bool SliderRow(const char* label, Vector3& value, float min, float max) {
        ImGui::PushID(label);
        RowLabel(label);
        bool changed = ImGui::SliderFloat3("##value", &value.x, min, max);
        ImGui::PopID();
        return changed;
    }

    inline bool ColorRow(const char* label, Color& color) {
        ImGui::PushID(label);
        RowLabel(label);
        bool changed = ImGui::ColorEdit4("##value", &color.x,
                                         ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
        ImGui::PopID();
        return changed;
    }

    inline bool CheckboxRow(const char* label, bool& value) {
        ImGui::PushID(label);
        bool changed = ImGui::Checkbox(label, &value);
        ImGui::PopID();
        return changed;
    }

    // Min and max sliders. Keeps min <= max the way a range picker does.
    inline bool RangeRows(const char* label, gfx::NumericRange<float>& range, float min, float max) {
        ImGui::PushID(label);
        ImGui::Dummy({ 0, 5 });
        ImGui::Text(label);

        bool changed = SliderRow("Min", range.min, min, max);
        changed |= SliderRow("Max", range.max, min, max);

        if (changed && range.min > range.max) std::swap(range.min, range.max);
        ImGui::PopID();
        return changed;
    }

    inline bool RangeRows(const char* label, gfx::NumericRange<int>& range, int min, int max) {
        ImGui::PushID(label);
        ImGui::Dummy({ 0, 5 });
        ImGui::Text(label);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Min");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::SliderInt("##min", &range.min, min, max);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Max");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::SliderInt("##max", &range.max, min, max);

        if (changed && range.min > range.max) std::swap(range.min, range.max);
        ImGui::PopID();
        return changed;
    }

    inline bool TextureRow(const char* label, TextureRegistry& registry, TexID& texture) {
        const char* current = "None";

        for (auto& entry : registry.Entries()) {
            if (entry.texid == texture) {
                current = entry.name.c_str();
                break;
            }
        }

        ImGui::PushID(label);
        RowLabel(label);

        bool changed = false;
        if (ImGui::BeginCombo("##value", current)) {
            if (ImGui::Selectable("None", texture == TexID::None)) {
                texture = TexID::None;
                changed = true;
            }

            for (auto& entry : registry.Entries()) {
                bool selected = entry.texid == texture;

                if (ImGui::Selectable(entry.name.c_str(), selected)) {
                    texture = entry.texid;
                    changed = true;
                }

                if (selected) ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::PopID();
        return changed;
    }

}

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
            _changed |= imgui::SliderRow("Position", _info.position, -50.0f, 50.0f);
            _changed |= imgui::SliderRow("Direction", _forward, -1.0f, 1.0f);
            _changed |= imgui::SliderRow("Cone Radius", _info.coneRadius, 0.0f, 1.0f);
            _changed |= imgui::SliderRow("Spawn Radius", _info.spawnRadius, 0.0f, 20.0f);

            ImGui::SeparatorText("Appearance");
            _changed |= imgui::SliderRow("Middle offset", _info.midOffset, 0, 1);
            _changed |= imgui::ColorRow("Start Color", _info.startColor);
            _changed |= imgui::ColorRow("Mid Color", _info.midColor);
            _changed |= imgui::ColorRow("End Color", _info.endColor);
            _changed |= imgui::SliderRow("Start Size", _info.startSize, 0.0f, 10.0f);
            _changed |= imgui::SliderRow("Mid Size", _info.midSize, 0.0f, 10.0f);
            _changed |= imgui::SliderRow("End Size", _info.endSize, 0.0f, 10.0f);
            _changed |= imgui::TextureRow("Texture", g_TextureRegistry, _info.texture);
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
