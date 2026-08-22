#pragma once
#include "ankerl/ankerl.h"
#include "d3/OutrageBitmap.h"
#include "Graphics/Texture.h"
#include "neon-types.h"
#include "Utility.h"
#include "shaders/Model.h"
#include "Graphics/GraphicsHandles.h"

namespace neon {

namespace d3 {
    struct VClip;
}

class VClipTable {
    List<d3::VClip> _vclips;
    CaseInsensitiveDictionary<string> _vclipFrameLookup; // maps vclip frames to the vclip file name

public:
    span<d3::VClip> Entries() { return _vclips; }

    void Add(const d3::VClip& vclip) {
        _vclips.push_back(vclip);
        for (auto& frame : vclip.frames) {
            _vclipFrameLookup[frame.name] = vclip.fileName;
        }
    }

    int FindIndex(string_view name) const {
        for (int i = 0; i < _vclips.size(); ++i) {
            if (_vclips[i].fileName == name) return i;
        }

        return -1;
    }

    const d3::VClip* Get(int index) const {
        return Seq::tryItem(_vclips, index);
    }

    // Returns the name of a vclip containing a given frame
    Option<string> FindByFrame(string_view frame) {
        auto fileNameOgf = frame + ".ogf";

        if (auto f = _vclipFrameLookup.find(frame); f != _vclipFrameLookup.end())
            return f->second;

        if (auto f = _vclipFrameLookup.find(fileNameOgf); f != _vclipFrameLookup.end())
            return f->second;

        return {};
    }
};

inline VClipTable g_VClips;


struct TextureEntry {
    string name;
    TexID texid = TexID::None;
    int handle = -1; // Index in the global texture descriptor table
    int vclip = -1; // vclip table index for this entry
    uint64 descriptor = 0; // GPU descriptor handle
    float opacity = 1;
};

class TextureRegistry {
    CaseInsensitiveDictionary<int> _textureLookup; // name -> texture index lookup
    List<TextureEntry> _textures; // texture with stable indices

public:
    bool IsLoaded(string_view name) const {
        return _textureLookup.contains(name);
    }

    const TextureEntry* Find(string_view name) const {
        if (auto find = _textureLookup.find(name); find != _textureLookup.end()) {
            return &_textures[find->second];
        }

        return nullptr;
    }

    void Upload(string_view name, const gfx::Image& image, float opacity = 1, int vclip = -1);

    void Clear() {
        for (auto& texture : _textures) {
            gfx::FreeTexture(texture.texid);
        }

        _textures.clear();
        _textureLookup.clear();
    }

    span<TextureEntry> Entries() { return _textures; }

    //void Free(string_view name) {
    //    gfx::UnloadTexture(entry.handle); // queue unload
    //}

    List<gfx::TextureInfo> BuildTextureTable(span<d3::VClip> vclips) {
        List<gfx::TextureInfo> table;

        for (auto& texture : _textures) {
            if (auto vclip = Seq::tryItem(vclips, texture.vclip)) {
                table.push_back({
                    .handle = texture.handle,
                    .frames = (int)vclip->frames.size(),
                    .frameTime = vclip->frameTime,
                    .pingpong = vclip->pingPong,
                    .opacity = texture.opacity
                });
            }
            else {
                table.push_back({ .handle = texture.handle, .opacity = texture.opacity });
            }
        }

        //for (auto& item : table) {
        //    SPDLOG_INFO("Texture handle {}", item.handle);
        //}

        return table;
    }

private:
    TextureEntry& GetEntry(string_view name) {
        if (_textureLookup.contains(name)) {
            auto index = _textureLookup[name];
            return _textures[index]; // this could fail if not kept in sync
        }
        else {
            // Allocate a new entry
            _textureLookup[name] = (int)_textures.size();
            return _textures.emplace_back();
        }
    }
};

extern TextureRegistry g_TextureRegistry;

//List<gfx::TexHandle> _loadedTextures; // GPU texture handle for each LoadedTexHandle
//List<TextureEntry> _textures; // Info for each LoadedTexHandle

//List<d3::VClip> _vclips; // vclips contain multiple frames that get stored into a single texture array
//CaseInsensitiveDictionary<string> _vclipFrameLookup; // maps vclip frames to the vclip file name

//int entryType = 2; // d1, d2, d3, filesystem
//int entry = -1; // Game table texture entry
//int handle = -1; // loaded texture handle 
//int vclipHandle = -1; // vclip table

}
