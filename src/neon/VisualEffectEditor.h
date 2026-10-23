#pragma once
#include <cstdio>
#include <memory>
#include <variant>
#include "d3/Hog2.h"
#include "Editor.h"
#include "neon-strings.h"
#include "Scene.h"
#include "TextureRegistry.h"
#include "imgui.h"
#include "VisualEffect.h"

namespace neon {

constexpr gfx::VisualEffect::Item CreateItem(gfx::EffectType type) {
    gfx::VisualEffect::Item item = { .type = type };

    switch (type) {
        case gfx::EffectType::Shockwave:
            item.data = gfx::ShockwaveEffect{};
            break;
        case gfx::EffectType::Sound:
            item.data = gfx::SoundEffect{};
            break;
        case gfx::EffectType::ParticleSystem:
            item.data = gfx::ParticleSystem{};
            break;
        default:
            throw Exception("CreateItem: Effect type is not defined");
    }

    return item;
}

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
    char _itemName[64]{};
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

    void SaveWithDialog() const;
    void LoadWithDialog();

    Item* Selected() noexcept {
        return Seq::tryItem(_items, _selection);
        /*return _selection >= 0 && _selection < (int)_items.size()
            ? &_items[_selection]
            : nullptr;*/
    }

    // Reloads the edit buffers from the given item.
    void SyncBuffers(const Item& item) {
        _forward = Vector3();
        _soundName[0] = '\0';

        size_t length = item.name.copy(_itemName, sizeof(_itemName) - 1);
        _itemName[length] = '\0';

        if (item.type == gfx::EffectType::Sound) {
            length = std::get<gfx::SoundEffect>(item.data).sound.copy(
                _soundName, sizeof(_soundName) - 1);
            _soundName[length] = '\0';
        }

        _syncedItem = &item;
    }

    // Properties shared by every effect type: the item's name and its
    // offset in the effect timeline.
    bool DrawCommonProperties(Item& item) {
        static float labelColumnWidth = 0;
        bool changed = false;

        ImGui::PushID("Common");
        imgui::RowLabel("Name", labelColumnWidth);
        if (ImGui::InputText("##value", _itemName, sizeof(_itemName))) {
            item.name = _itemName;
            changed = true;
        }
        ImGui::PopID();

        changed |= imgui::SliderRow("Time Offset", item.timeOffset, 0.0f, 10.0f, labelColumnWidth);

        return changed;
    }

    bool DrawParticleSystemProperties(gfx::ParticleSystem& system) {
        bool changed = false;
        static float labelColumnWidth = 0;

        auto& info = system.info;

        ImGui::SeparatorText("Particle System");
        changed |= imgui::SliderRow("Lifespan", info.lifespan, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Offset", info.position, -10.0f, 10.0f, labelColumnWidth);

        if (imgui::SliderRow("Direction", _forward, -1.0f, 1.0f, labelColumnWidth)) {
            info.SetForwardDirection(_forward);
            changed = true;
        }

        changed |= imgui::SliderRow("Cone Radius", info.coneRadius, 0.0f, 1.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Spawn Radius", info.spawnRadius, 0.0f, 10.0f, labelColumnWidth);

        ImGui::SeparatorText("Appearance");
        changed |= imgui::SliderRow("Middle offset", info.midOffset, 0, 1, labelColumnWidth);
        changed |= imgui::ColorRow("Start Color", info.startColor, labelColumnWidth);
        changed |= imgui::ColorRow("Mid Color", info.midColor, labelColumnWidth);
        changed |= imgui::ColorRow("End Color", info.endColor, labelColumnWidth);
        changed |= imgui::SliderRow("Start Size", info.startSize, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Mid Size", info.midSize, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::SliderRow("End Size", info.endSize, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::TextureRow("Texture", g_TextureRegistry, info.texture, labelColumnWidth);
        changed |= imgui::BlendRow("Blend mode", info.blendMode, labelColumnWidth);
        changed |= imgui::CheckboxRow("Stretch Animation", info.stretchAnimation, labelColumnWidth);
        changed |= imgui::CheckboxRow("Random Rotation", info.randomRotation, labelColumnWidth);
        changed |= imgui::RangeRows("Rotation speed", info.rotationSpeed, -5.0f, 5.0f, labelColumnWidth);

        ImGui::SeparatorText("Motion");
        changed |= imgui::SliderRow("Gravity", info.gravity, -10.0f, 10.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Wind", info.wind, -10.0f, 10.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Drag", info.drag, 0.0f, 1.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Restitution", info.restitution, 0.0f, 1.0f, labelColumnWidth);
        changed |= imgui::SliderRow("Spawn Radius", info.spawnRadius, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::CheckboxRow("Use Collision", info.useCollision, labelColumnWidth);

        ImGui::SeparatorText("Emission");
        changed |= imgui::RangeRows("Count", info.count, 1, 10, labelColumnWidth);
        changed |= imgui::RangeRows("Velocity", info.velocity, 0.0f, 50.0f, labelColumnWidth);
        changed |= imgui::RangeRows("Duration", info.duration, 0.0f, 10.0f, labelColumnWidth);
        changed |= imgui::RangeRows("Interval", info.interval, 0.0f, 1.0f, labelColumnWidth);

        return changed;
    }

    bool DrawShockwaveProperties(gfx::ShockwaveEffect& effect) {
        static float _labelColumnWidth = 0;
        bool changed = false;

        ImGui::SeparatorText("Shockwave");
        changed |= imgui::SliderRow("Position", effect.position, -10.0f, 10.0f, _labelColumnWidth);
        changed |= imgui::SliderRow("Duration", effect.duration, 0.0f, 2.0f, _labelColumnWidth);
        changed |= imgui::SliderRow("Radius", effect.radius, 0.0f, 20.0f, _labelColumnWidth);
        changed |= imgui::SliderRow("Count", effect.count, 1, 10, _labelColumnWidth);
        changed |= imgui::ColorRow("Color", effect.color, _labelColumnWidth);
        changed |= imgui::TextureRow("Texture", g_TextureRegistry, effect.texture, _labelColumnWidth);
        changed |= imgui::CheckboxRow("Random Rotation", effect.randomRotation, _labelColumnWidth);
        changed |= imgui::CheckboxRow("Face Camera", effect.faceCamera, _labelColumnWidth);

        return changed;
    }

    bool DrawSoundProperties(gfx::SoundEffect& sound, const List<d3::Hog2::Entry>& sounds) {
        static float _labelColumnWidth = 0;
        bool changed = false;

        ImGui::PushID("Sound");
        ImGui::SeparatorText("Sound");

        imgui::RowLabel("File", _labelColumnWidth);
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

        changed |= imgui::SliderRow("Volume", sound.volume, 0.0f, 2.0f, _labelColumnWidth);
        changed |= imgui::SliderRow("Pitch", sound.pitch, 0.0f, 4.0f, _labelColumnWidth);
        changed |= imgui::CheckboxRow("Looping", sound.looping, _labelColumnWidth);

        return changed;
    }

    void Play(Scene& scene) {
        if (_items.empty()) return;

        scene.effects.clear();
        scene.shockwaves.clear();
        scene.particles.clear();

        gfx::VisualEffect effect;
        effect.items = _items;

        for (auto& existing : scene.effects) {
            if (existing.elapsed > existing.duration) {
                existing = effect;
                return;
            }
        }

        scene.effects.push_back(effect);
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

    void Duplicate(int index) {
        if (index < 0 || index >= (int)_items.size()) return;

        Item copy = _items[index];
        _items.insert(_items.begin() + index + 1, std::move(copy));
        _selection = index + 1;
        _syncedItem = nullptr;
    }

    void Delete(int index) {
        if (index < 0 || index >= (int)_items.size()) return;

        _items.erase(_items.begin() + index);
        if (_selection == index) _syncedItem = nullptr;
        if (_selection >= (int)_items.size()) _selection = (int)_items.size() - 1;
    }

    void ListActions(Scene& scene) {
        //float buttonWidth = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
        float buttonWidth = 100;

        if (ImGui::Button("Add", { buttonWidth, 0 }))
            ImGui::OpenPopup("AddEffect");

        ImGui::SameLine();
        bool hasSelection = Selected() != nullptr;
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Copy", { buttonWidth, 0 }))
            Duplicate(_selection);
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Delete", { buttonWidth, 0 }))
            Delete(_selection);
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(_items.empty());
        if (ImGui::Button("Play", { buttonWidth, 0 }))
            Play(scene);
        ImGui::EndDisabled();

        ImGui::SameLine();
        float actionsWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - actionsWidth);

        if (ImGui::Button("Save", { buttonWidth, 0 }))
            SaveWithDialog();

        ImGui::SameLine();
        if (ImGui::Button("Load", { buttonWidth, 0 }))
            LoadWithDialog();

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
    bool Draw(Scene& scene, const List<d3::Hog2::Entry>& sounds, const char* title = "Visual Effect Editor") {
        _changed = false;
        if (!_visible) return false;

        if (ImGui::Begin(title, &_visible)) {
            {
                float actionsHeight = ImGui::GetFrameHeight() * 1 + ImGui::GetStyle().ItemSpacing.y /*+ ImGui::GetStyle().WindowPadding.y * 1*/;
                ImGui::BeginChild("actions", { 0, actionsHeight });
                ListActions(scene);
                ImGui::EndChild();
            }

            constexpr auto EFFECT_COLUMN_WIDTH = 220;

            {
                // Effect list
                // -ImGui::GetFrameHeightWithSpacing()
                ImGui::BeginChild("effects", { EFFECT_COLUMN_WIDTH, 0 }, ImGuiChildFlags_Borders);

                for (int i = 0; i < (int)_items.size(); ++i) {
                    ImGui::PushID(i);
                    bool selected = i == _selection;
                    const Item& entry = _items[i];
                    const char* name = entry.name.empty() ? EffectTypeName(entry.type) : entry.name.c_str();

                    char label[160];
                    std::snprintf(label, sizeof(label), "%s (%.2fs)", name, entry.timeOffset);

                    if (ImGui::Selectable(label, selected))
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

                _changed |= DrawCommonProperties(*item);

                switch (item->type) {
                    case gfx::EffectType::ParticleSystem:
                        _changed |= DrawParticleSystemProperties(std::get<gfx::ParticleSystem>(item->data));
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
