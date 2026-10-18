#pragma once
#include <algorithm>
#include "Graphics/ParticleSystem.h"
#include "TextureRegistry.h"
#include "imgui.h"

namespace neon::imgui {

// Draws the label of a property row and moves the cursor to the shared value
// column. The caller owns labelColumnWidth and passes the same variable to
// every row in a panel so their value widgets line up.
inline float DrawLabelRow(const char* label, float& labelColumnWidth) {
    float x = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    labelColumnWidth = std::max(labelColumnWidth, ImGui::CalcTextSize(label).x);
    ImGui::SameLine(x + labelColumnWidth + ImGui::GetStyle().ItemInnerSpacing.x);
    return x;
}

inline void RowLabel(const char* label, float& labelColumnWidth) {
    DrawLabelRow(label, labelColumnWidth);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

inline bool SliderRow(const char* label, uint& value, uint min, uint max, float& labelColumnWidth) {
    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);

    int v = (int)value;
    bool changed = ImGui::SliderInt("##value", &v, min, max);
    if (changed) {
        value = v;
    }

    ImGui::PopID();
    return changed;
}

inline bool SliderRow(const char* label, int& value, int min, int max, float& labelColumnWidth) {
    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);
    bool changed = ImGui::SliderInt("##value", &value, min, max);
    ImGui::PopID();
    return changed;
}

inline bool SliderRow(const char* label, float& value, float min, float max, float& labelColumnWidth) {
    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);
    bool changed = ImGui::SliderFloat("##value", &value, min, max);
    ImGui::PopID();
    return changed;
}

inline bool SliderRow(const char* label, Vector3& value, float min, float max, float& labelColumnWidth) {
    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);
    bool changed = ImGui::SliderFloat3("##value", &value.x, min, max);
    ImGui::PopID();
    return changed;
}

inline bool ColorRow(const char* label, Color& color, float& labelColumnWidth) {
    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);
    bool changed = ImGui::ColorEdit4("##value", &color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    ImGui::PopID();
    return changed;
}

inline bool CheckboxRow(const char* label, bool& value, float& labelColumnWidth) {
    ImGui::PushID(label);
    DrawLabelRow(label, labelColumnWidth);
    bool changed = ImGui::Checkbox("##value", &value);
    ImGui::PopID();
    return changed;
}

// Min and max sliders. Keeps min <= max the way a range picker does.
inline bool RangeRows(const char* label, gfx::NumericRange<float>& range, float min, float max, float& labelColumnWidth) {
    ImGui::PushID(label);
    ImGui::Dummy({ 0, 5 });
    ImGui::Text(label);

    bool changed = SliderRow("Min", range.min, min, max, labelColumnWidth);
    changed |= SliderRow("Max", range.max, min, max, labelColumnWidth);

    if (changed && range.min > range.max) std::swap(range.min, range.max);
    ImGui::PopID();
    return changed;
}

inline bool RangeRows(const char* label, gfx::NumericRange<int>& range, int min, int max, float& labelColumnWidth) {
    ImGui::PushID(label);
    ImGui::Dummy({ 0, 5 });
    ImGui::Text(label);

    bool changed = SliderRow("Min", range.min, min, max, labelColumnWidth);
    changed |= SliderRow("Max", range.max, min, max, labelColumnWidth);

    if (changed && range.min > range.max) std::swap(range.min, range.max);
    ImGui::PopID();
    return changed;
}

inline const char* BlendModeName(gfx::BlendMode mode) {
    switch (mode) {
        case gfx::BlendMode::Alpha: return "Alpha";
        case gfx::BlendMode::Additive: return "Additive";
        case gfx::BlendMode::Multiply: return "Multiply";
        default: return "Other";
    }
}

inline bool BlendRow(const char* label, gfx::BlendMode& mode, float& labelColumnWidth) {
    const char* current = BlendModeName(mode);

    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);

    bool changed = false;
    if (ImGui::BeginCombo("##value", current)) {
        for (gfx::BlendMode candidate : { gfx::BlendMode::Alpha, gfx::BlendMode::Additive, gfx::BlendMode::Multiply }) {
            bool selected = mode == candidate;

            if (ImGui::Selectable(BlendModeName(candidate), selected)) {
                mode = candidate;
                changed = true;
            }

            if (selected) ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
    return changed;
}

inline bool TextureRow(const char* label, TextureRegistry& registry, TexID& texture, float& labelColumnWidth) {
    const char* current = "None";

    for (auto& entry : registry.Entries()) {
        if (entry.texid == texture) {
            current = entry.name.c_str();
            break;
        }
    }

    ImGui::PushID(label);
    RowLabel(label, labelColumnWidth);

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
