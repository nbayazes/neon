#pragma once
#include <cstdio>
#include <memory>
#include <variant>
#include "d3/Hog2.h"
#include "Editor.h"
#include "neon-strings.h"
#include "TextureRegistry.h"
#include "imgui.h"
#include "VisualEffect.h"

namespace neon {

// Window with a list of visual effect items (particle systems, shockwaves
// and sounds). Each item has a timeline offset, can be added or deleted
// and selecting one shows its properties in the panel on the right.
class VisualEffectEditor {
    using Item = gfx::VisualEffect::Item;

    List<Item> _items;
    int _selection = -1;
    bool _visible = false;
    bool _changed = false;

    Vector3 _forward; // edit buffer for the selected particle system's direction
    const Item* _syncedItem = nullptr; // item the edit buffers below were loaded from
    char _soundName[128]{};
    char _soundSearch[64]{};

    static const char* EffectTypeName(gfx::EffectType type) {
        switch (type) {
            case gfx::EffectType::ParticleSystem: return "Particle System";
            case gfx::EffectType::Shockwave: return "Shockwave";
            case gfx::EffectType::Sound: return "Sound";
            default: return "None";
        }
    }

    // Replaces the data of an item with a fresh value of the given type.
    static void SetEffectType(Item& item, gfx::EffectType type) {
        switch (type) {
            case gfx::EffectType::Shockwave:
                item.data = gfx::ShockwaveEffect{};
                break;
            case gfx::EffectType::Sound:
                item.data = gfx::SoundEffect{};
                break;
            default:
                item.data = gfx::ParticleSystemInfo{};
                break;
        }

        item.type = type;
    }

    static Item CreateItem(gfx::EffectType type) {
        Item item;
        SetEffectType(item, type);
        return item;
    }

    Item* Selected() noexcept {
        return _selection >= 0 && _selection < (int)_items.size()
            ? &_items[_selection]
            : nullptr;
    }

    // Reloads the edit buffers from the given item.
    void SyncBuffers(const Item& item) {
        _forward = {};
        _soundName[0] = '\0';

        if (item.type == gfx::EffectType::Sound) {
            size_t length = std::get<gfx::SoundEffect>(item.data).sound.copy(
                _soundName, sizeof(_soundName) - 1);
            _soundName[length] = '\0';
        }

        _syncedItem = &item;
    }

    //bool DrawTypeRow(Item& item) {
    //    ImGui::PushID("Type");
    //    imgui::RowLabel("Type");

    //    bool changed = false;
    //    if (ImGui::BeginCombo("##value", EffectTypeName(item.type))) {
    //        for (gfx::EffectType candidate : {
    //                 gfx::EffectType::ParticleSystem, gfx::EffectType::Shockwave, gfx::EffectType::Sound
    //             }) {
    //            bool selected = item.type == candidate;

    //            if (ImGui::Selectable(EffectTypeName(candidate), selected) && !selected) {
    //                SetEffectType(item, candidate);
    //                SyncBuffers(item);
    //                changed = true;
    //            }

    //            if (selected) ImGui::SetItemDefaultFocus();
    //        }

    //        ImGui::EndCombo();
    //    }

    //    ImGui::PopID();
    //    return changed;
    //}

    bool DrawParticleProperties(gfx::ParticleSystemInfo& info) {
        bool changed = false;

        ImGui::SeparatorText("Transform");
        changed |= imgui::SliderRow("Position", info.position, -10.0f, 10.0f);

        if (imgui::SliderRow("Direction", _forward, -1.0f, 1.0f)) {
            info.SetForwardDirection(_forward);
            changed = true;
        }

        changed |= imgui::SliderRow("Cone Radius", info.coneRadius, 0.0f, 1.0f);
        changed |= imgui::SliderRow("Spawn Radius", info.spawnRadius, 0.0f, 10.0f);

        ImGui::SeparatorText("Appearance");
        changed |= imgui::SliderRow("Middle offset", info.midOffset, 0, 1);
        changed |= imgui::ColorRow("Start Color", info.startColor);
        changed |= imgui::ColorRow("Mid Color", info.midColor);
        changed |= imgui::ColorRow("End Color", info.endColor);
        changed |= imgui::SliderRow("Start Size", info.startSize, 0.0f, 10.0f);
        changed |= imgui::SliderRow("Mid Size", info.midSize, 0.0f, 10.0f);
        changed |= imgui::SliderRow("End Size", info.endSize, 0.0f, 10.0f);
        changed |= imgui::TextureRow("Texture", g_TextureRegistry, info.texture);
        changed |= imgui::BlendRow("Blend mode", info.blendMode);
        changed |= imgui::CheckboxRow("Stretch Animation", info.stretchAnimation);
        changed |= imgui::CheckboxRow("Random Rotation", info.randomRotation);
        changed |= imgui::RangeRows("Rotation speed", info.rotationSpeed, -5.0f, 5.0f);

        ImGui::SeparatorText("Motion");
        changed |= imgui::SliderRow("Gravity", info.gravity, -10.0f, 10.0f);
        changed |= imgui::SliderRow("Wind", info.wind, -10.0f, 10.0f);
        changed |= imgui::SliderRow("Drag", info.drag, 0.0f, 1.0f);
        changed |= imgui::SliderRow("Restitution", info.restitution, 0.0f, 1.0f);
        changed |= imgui::SliderRow("Spawn Radius", info.spawnRadius, 0.0f, 10.0f);
        changed |= imgui::CheckboxRow("Use Collision", info.useCollision);

        ImGui::SeparatorText("Emission");
        changed |= imgui::RangeRows("Count", info.count, 0, 100);
        changed |= imgui::RangeRows("Velocity", info.velocity, 0.0f, 50.0f);
        changed |= imgui::RangeRows("Duration", info.duration, 0.0f, 10.0f);
        changed |= imgui::RangeRows("Interval", info.interval, 0.0f, 1.0f);

        return changed;
    }

    bool DrawShockwaveProperties(gfx::ShockwaveEffect& effect) {
        bool changed = false;

        ImGui::SeparatorText("Appearance");
        changed |= imgui::SliderRow("Position", effect.position, -10.0f, 10.0f);
        changed |= imgui::SliderRow("Duration", effect.duration, 0.0f, 2.0f);
        changed |= imgui::SliderRow("Radius", effect.radius, 0.0f, 20.0f);
        changed |= imgui::SliderRow("Count", effect.count, 1, 10);
        changed |= imgui::TextureRow("Texture", g_TextureRegistry, effect.texture);
        changed |= imgui::CheckboxRow("Random Rotation", effect.randomRotation);
        changed |= imgui::CheckboxRow("Face Camera", effect.faceCamera);
        changed |= imgui::ColorRow("Color", effect.color);

        return changed;
    }

    bool DrawSoundProperties(gfx::SoundEffect& sound, const List<d3::Hog2::Entry>& sounds) {
        bool changed = false;

        ImGui::PushID("Sound");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Name");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-ImGui::GetFrameHeightWithSpacing());
        if (ImGui::InputText("##value", _soundName, sizeof(_soundName))) {
            sound.sound = _soundName;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("..."))
            ImGui::OpenPopup("SoundPicker");

        ImGui::SetNextWindowSize({ 350, 400 });
        if (ImGui::BeginPopup("SoundPicker")) {
            ImGui::Text("Search");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##Search", _soundSearch, sizeof(_soundSearch));
            auto searchstr = String::ToLower(_soundSearch);

            if (ImGui::BeginChild("sounds", { -1, -1 }, true)) {
                for (auto& entry : sounds) {
                    if (!searchstr.empty() && !String::Contains(String::ToLower(entry.name), searchstr))
                        continue;

                    if (ImGui::Selectable(entry.name.c_str())) {
                        std::snprintf(_soundName, sizeof(_soundName), "%s", entry.name.c_str());
                        sound.sound = entry.name;
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();

        changed |= imgui::SliderRow("Volume", sound.volume, 0.0f, 2.0f);
        changed |= imgui::SliderRow("Pitch", sound.pitch, 0.0f, 4.0f);
        changed |= imgui::CheckboxRow("Looping", sound.looping);

        return changed;
    }

public:
    span<const Item> Items() const noexcept { return _items; }

    int SelectedIndex() const noexcept { return _selection; }

    bool Visible() const noexcept { return _visible; }
    void Show(bool visible = true) noexcept { _visible = visible; }

    void Toggle() noexcept { _visible = !_visible; }

    void Add(gfx::EffectType type) {
        _items.push_back(CreateItem(type));
        _selection = (int)_items.size() - 1;
    }

    void Delete(int index) {
        if (index < 0 || index >= (int)_items.size()) return;

        _items.erase(_items.begin() + index);
        if (_selection == index) _syncedItem = nullptr;
        if (_selection >= (int)_items.size()) _selection = (int)_items.size() - 1;
    }

    void ListActions() {
        if (ImGui::Button("Add", { 100, 0 }))
            ImGui::OpenPopup("AddEffect");

        ImGui::SameLine();
        bool hasSelection = Selected() != nullptr;
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Delete", { 100, 0 }))
            Delete(_selection);
        ImGui::EndDisabled();

        if (ImGui::BeginPopup("AddEffect")) {
            for (gfx::EffectType type : {
                gfx::EffectType::ParticleSystem, gfx::EffectType::Shockwave, gfx::EffectType::Sound
            }) {
                if (ImGui::Selectable(EffectTypeName(type)))
                    Add(type);
            }
            ImGui::EndPopup();
        }
    }

    // Draws the window. Returns true when a property changed since the last draw.
    bool Draw(const List<d3::Hog2::Entry>& sounds, const char* title = "Visual Effect Editor") {
        _changed = false;
        if (!_visible) return false;

        if (ImGui::Begin(title, &_visible)) {
            constexpr auto EFFECT_COLUMN_WIDTH = 220;
            {
                ImGui::BeginChild("actions", { EFFECT_COLUMN_WIDTH, 35 });
                ListActions();
                ImGui::EndChild();
            }

            {
                // Effect list
                // -ImGui::GetFrameHeightWithSpacing()
                ImGui::BeginChild("effects", { EFFECT_COLUMN_WIDTH, 0 }, ImGuiChildFlags_Borders);

                for (int i = 0; i < (int)_items.size(); ++i) {
                    ImGui::PushID(i);
                    bool selected = i == _selection;
                    if (ImGui::Selectable(EffectTypeName(_items[i].type), selected))
                        _selection = i;
                    if (selected) ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }

            ImGui::SameLine();
            ImGui::BeginChild("properties", { -1, -1 }, ImGuiChildFlags_Borders);
            if (Item* item = Selected()) {
                if (_syncedItem != item)
                    SyncBuffers(*item);

                //_changed |= DrawTypeRow(*item);

                ImGui::SeparatorText("Timeline");
                _changed |= imgui::SliderRow("Time Offset", item->timeOffset, 0.0f, 10.0f);

                switch (item->type) {
                    case gfx::EffectType::ParticleSystem:
                        _changed |= DrawParticleProperties(std::get<gfx::ParticleSystemInfo>(item->data));
                        break;
                    case gfx::EffectType::Shockwave:
                        _changed |= DrawShockwaveProperties(std::get<gfx::ShockwaveEffect>(item->data));
                        break;
                    case gfx::EffectType::Sound:
                        _changed |= DrawSoundProperties(std::get<gfx::SoundEffect>(item->data), sounds);
                        break;
                }
            }
            //else {
            //    ImGui::TextDisabled("Select an effect or add a new one");
            //}
            ImGui::EndChild();
        }

        ImGui::End();
        return _changed;
    }
};

}
