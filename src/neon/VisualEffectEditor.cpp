#include "pch.h"
#include "VisualEffectEditor.h"

#include <fstream>
#include <iterator>

#include "WindowsDialogs.h"
#pragma warning(push)
#pragma warning(disable: 4702) // unreachable code in c4/ext/fast_float_all.h
#include "Yaml.h"
#pragma warning(pop)

namespace neon {

namespace {

    using Item = gfx::VisualEffect::Item;

    void WriteRange(ryml::NodeRef node, const char* name, const gfx::NumericRange<float>& range) {
        auto child = node[name];
        child |= ryml::MAP;
        child["Min"] << range.min;
        child["Max"] << range.max;
    }

    void WriteRange(ryml::NodeRef node, const char* name, const gfx::NumericRange<int>& range) {
        auto child = node[name];
        child |= ryml::MAP;
        child["Min"] << range.min;
        child["Max"] << range.max;
    }

    template <class T>
    void ReadRange(ryml::ConstNodeRef node, const char* name, gfx::NumericRange<T>& range) {
        auto child = Yaml::GetNode(node, name);
        if (!child) return;

        Yaml::ReadValue2(*child, "Min", range.min);
        Yaml::ReadValue2(*child, "Max", range.max);
    }

    void WriteTexture(ryml::NodeRef node, TexID texture) {
        string name = "None";

        if (texture != TexID::None) {
            for (auto& entry : g_TextureRegistry.Entries()) {
                if (entry.texid == texture) {
                    name = entry.name;
                    break;
                }
            }
        }

        node["Texture"] << name;
    }

    void ReadTexture(ryml::ConstNodeRef node, TexID& texture) {
        std::string name;
        texture = TexID::None;

        if (!Yaml::ReadValue2(node, "Texture", name) || name == "None") return;
        if (const TextureEntry* entry = g_TextureRegistry.Find(name))
            texture = entry->texid;
    }

    void WriteParticleSystem(ryml::NodeRef node, const gfx::ParticleSystem& system) {
        const auto& info = system.info;

        node["Lifespan"] << info.lifespan;
        node["Position"] << Yaml::EncodeVector(info.position);
        node["Right"] << Yaml::EncodeVector(info.orientation.Right());
        node["Up"] << Yaml::EncodeVector(info.orientation.Up());
        // Backward() is used here because it returns the raw third column in the matrix.
        // The Matrix3x3 constructor does not negate the vector.
        node["Backward"] << Yaml::EncodeVector(info.orientation.Backward());
        node["ConeRadius"] << info.coneRadius;
        node["SpawnRadius"] << info.spawnRadius;
        node["MidOffset"] << info.midOffset;
        node["StartColor"] << Yaml::EncodeColor(info.startColor);
        node["MidColor"] << Yaml::EncodeColor(info.midColor);
        node["EndColor"] << Yaml::EncodeColor(info.endColor);
        node["StartSize"] << info.startSize;
        node["MidSize"] << info.midSize;
        node["EndSize"] << info.endSize;
        node["Gravity"] << Yaml::EncodeVector(info.gravity);
        node["Wind"] << Yaml::EncodeVector(info.wind);
        node["Drag"] << info.drag;
        node["Restitution"] << info.restitution;
        node["UseCollision"] << info.useCollision;
        node["StretchAnimation"] << info.stretchAnimation;
        node["RandomRotation"] << info.randomRotation;
        node["BlendMode"] << (int)info.blendMode;

        WriteRange(node, "Count", info.count);
        WriteRange(node, "Velocity", info.velocity);
        WriteRange(node, "Duration", info.duration);
        WriteRange(node, "Interval", info.interval);
        WriteRange(node, "RotationSpeed", info.rotationSpeed);
        WriteTexture(node, info.texture);
    }

    void ReadParticleSystem(ryml::ConstNodeRef node, gfx::ParticleSystem& system) {
        auto& info = system.info;

        Yaml::ReadValue2(node, "Lifespan", info.lifespan);
        Yaml::ReadValue2(node, "Position", info.position);

        Vector3 right, up, backward;
        if (Yaml::ReadValue2(node, "Right", right) &&
            Yaml::ReadValue2(node, "Up", up) &&
            Yaml::ReadValue2(node, "Backward", backward)) {
            info.orientation = Matrix3x3(right, up, backward);
        }

        Yaml::ReadValue2(node, "ConeRadius", info.coneRadius);
        Yaml::ReadValue2(node, "SpawnRadius", info.spawnRadius);
        Yaml::ReadValue2(node, "MidOffset", info.midOffset);
        Yaml::ReadValue2(node, "StartColor", info.startColor);
        Yaml::ReadValue2(node, "MidColor", info.midColor);
        Yaml::ReadValue2(node, "EndColor", info.endColor);
        Yaml::ReadValue2(node, "StartSize", info.startSize);
        Yaml::ReadValue2(node, "MidSize", info.midSize);
        Yaml::ReadValue2(node, "EndSize", info.endSize);
        Yaml::ReadValue2(node, "Gravity", info.gravity);
        Yaml::ReadValue2(node, "Wind", info.wind);
        Yaml::ReadValue2(node, "Drag", info.drag);
        Yaml::ReadValue2(node, "Restitution", info.restitution);
        Yaml::ReadValue2(node, "UseCollision", info.useCollision);
        Yaml::ReadValue2(node, "StretchAnimation", info.stretchAnimation);
        Yaml::ReadValue2(node, "RandomRotation", info.randomRotation);

        int blendMode = (int)info.blendMode;
        if (Yaml::ReadValue2(node, "BlendMode", blendMode))
            info.blendMode = (gfx::BlendMode)blendMode;

        ReadRange(node, "Count", info.count);
        ReadRange(node, "Velocity", info.velocity);
        ReadRange(node, "Duration", info.duration);
        ReadRange(node, "Interval", info.interval);
        ReadRange(node, "RotationSpeed", info.rotationSpeed);
        ReadTexture(node, info.texture);
    }

    void WriteShockwave(ryml::NodeRef node, const gfx::ShockwaveEffect& effect) {
        node["Position"] << Yaml::EncodeVector(effect.position);
        // Save the raw rows so the rotation matrix round-trips exactly (see above).
        node["Right"] << Yaml::EncodeVector(effect.rotation.Right());
        node["Up"] << Yaml::EncodeVector(effect.rotation.Up());
        node["Backward"] << Yaml::EncodeVector(effect.rotation.Backward());
        node["Duration"] << effect.duration;
        node["Radius"] << effect.radius;
        node["Count"] << effect.count;
        node["RandomRotation"] << effect.randomRotation;
        node["FaceCamera"] << effect.faceCamera;
        node["CameraRotation"] << effect.cameraRotation;
        node["Color"] << Yaml::EncodeColor(effect.color);

        WriteTexture(node, effect.texture);
    }

    void ReadShockwave(ryml::ConstNodeRef node, gfx::ShockwaveEffect& effect) {
        Yaml::ReadValue2(node, "Position", effect.position);

        Vector3 right, up, backward;
        if (Yaml::ReadValue2(node, "Right", right) &&
            Yaml::ReadValue2(node, "Up", up) &&
            Yaml::ReadValue2(node, "Backward", backward)) {
            effect.rotation = Matrix3x3(right, up, backward);
        }

        Yaml::ReadValue2(node, "Duration", effect.duration);
        Yaml::ReadValue2(node, "Radius", effect.radius);
        Yaml::ReadValue2(node, "Count", effect.count);
        Yaml::ReadValue2(node, "RandomRotation", effect.randomRotation);
        Yaml::ReadValue2(node, "FaceCamera", effect.faceCamera);
        Yaml::ReadValue2(node, "CameraRotation", effect.cameraRotation);
        Yaml::ReadValue2(node, "Color", effect.color);
        ReadTexture(node, effect.texture);
    }

    void WriteSound(ryml::NodeRef node, const gfx::SoundEffect& sound) {
        node["Position"] << Yaml::EncodeVector(sound.position);
        node["Sound"] << sound.sound;
        node["Volume"] << sound.volume;
        node["Pitch"] << sound.pitch;
        node["Looping"] << sound.looping;
    }

    void ReadSound(ryml::ConstNodeRef node, gfx::SoundEffect& sound) {
        Yaml::ReadValue2(node, "Position", sound.position);
        Yaml::ReadValue2(node, "Sound", sound.sound);
        Yaml::ReadValue2(node, "Volume", sound.volume);
        Yaml::ReadValue2(node, "Pitch", sound.pitch);
        Yaml::ReadValue2(node, "Looping", sound.looping);
    }

    void WriteItem(ryml::NodeRef node, const Item& item) {
        node |= ryml::MAP;
        node["Type"] << (int)item.type;
        node["Name"] << item.name;
        node["TimeOffset"] << item.timeOffset;

        auto data = node["Data"];
        data |= ryml::MAP;

        switch (item.type) {
            case gfx::EffectType::ParticleSystem:
                WriteParticleSystem(data, std::get<gfx::ParticleSystem>(item.data));
                break;
            case gfx::EffectType::Shockwave:
                WriteShockwave(data, std::get<gfx::ShockwaveEffect>(item.data));
                break;
            case gfx::EffectType::Sound:
                WriteSound(data, std::get<gfx::SoundEffect>(item.data));
                break;
            default:
                break;
        }
    }

    void ReadItemData(ryml::ConstNodeRef node, Item& item) {
        switch (item.type) {
            case gfx::EffectType::ParticleSystem:
                ReadParticleSystem(node, std::get<gfx::ParticleSystem>(item.data));
                break;
            case gfx::EffectType::Shockwave:
                ReadShockwave(node, std::get<gfx::ShockwaveEffect>(item.data));
                break;
            case gfx::EffectType::Sound:
                ReadSound(node, std::get<gfx::SoundEffect>(item.data));
                break;
            default:
                break;
        }
    }

}

std::string Serialize(span<const Item> items) {
    ryml::Tree tree;
    auto root = tree.rootref();
    root |= ryml::MAP;

    auto effects = root["Effects"];
    effects |= ryml::SEQ;

    for (const auto& item : items) {
        auto child = effects.append_child();
        WriteItem(child, item);
    }

    return ryml::emitrs_yaml<std::string>(tree);
}

List<Item> Deserialize(std::string_view yaml) {
    std::string text(yaml);

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(text));
    auto effects = Yaml::GetSequenceNode(tree.rootref(), "Effects");
    if (!effects) throw Exception("File does not contain a visual effect list");

    List<Item> items;
    for (auto child : effects->children()) {
        if (!child.is_map()) continue;

        int type = (int)gfx::EffectType::None;
        Yaml::ReadValue2(child, "Type", type);
        if (type <= (int)gfx::EffectType::None || type > (int)gfx::EffectType::Sound)
            continue;

        Item item = CreateItem((gfx::EffectType)type);
        Yaml::ReadValue2(child, "Name", item.name);
        Yaml::ReadValue2(child, "TimeOffset", item.timeOffset);

        if (auto data = Yaml::GetNode(child, "Data"))
            ReadItemData(*data, item);

        items.push_back(std::move(item));
    }

    return items;
}

void SaveFile(const filesystem::path& path, span<const Item> items) {
    auto text = Serialize(items);

    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw Exception("Could not open file for writing");

    stream.write(text.data(), (std::streamsize)text.size());
    if (!stream) throw Exception("Could not write file");
}

List<gfx::VisualEffect::Item> LoadFile(const filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw Exception("Could not open file");

    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return Deserialize(text);
}

void VisualEffectEditor::SaveWithDialog() const {
    constexpr DialogFilter filters[] = { { L"YAML Files (*.yml)", L"*.yml" } };
    auto path = SaveFileDialog(filters, 1, "visual_effects.yml", "Save Visual Effects");
    if (!path) return;

    try {
        SaveFile(*path, _items);
    }
    catch (const std::exception& e) {
        ShowErrorMessage(e);
    }
}

void VisualEffectEditor::LoadWithDialog() {
    constexpr DialogFilter filters[] = { { L"YAML Files (*.yml)", L"*.yml" } };
    auto path = OpenFileDialog(filters, "Load Visual Effects");
    if (!path) return;

    try {
        auto items = LoadFile(*path);
        _items = std::move(items);
        _selection = _items.empty() ? -1 : 0;
        _syncedItem = nullptr;
    }
    catch (const std::exception& e) {
        ShowErrorMessage(e);
    }
}

}
