#include "pch.h"
#include "Application.h"
#include <spdlog/spdlog.h>
#include "d3/Hog2.h"
#include "d3/OutrageBitmap.h"
#include "d3/OutrageModel.h"
#include "d3/OutrageTable.h"
#include "Graphics/Graphics.h"
#include "Graphics/Image.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "shaders/ModelVertex.h"
#include "SystemClock.h"

namespace neon::app {

void Update() {}

void PopulateTangents(span<gfx::shaders::ModelVertex> verts) {
    auto edge1 = verts[1].position - verts[0].position;
    auto edge2 = verts[2].position - verts[0].position;
    auto deltaUV1 = verts[1].uv - verts[0].uv;
    auto deltaUV2 = verts[2].uv - verts[0].uv;

    static_assert(std::numeric_limits<float>::is_iec559); // Check that nan / inf behavior is defined
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    if (std::isnan(f) || std::isinf(f)) {
        // Invalid UVs or untextured side
        edge1.Normalize(verts[0].tangent);
        verts[1].tangent = verts[2].tangent = verts[0].tangent;
        auto bitangent = verts[0].tangent.Cross(verts[0].normal);
        verts[0].bitangent = verts[1].bitangent = verts[2].bitangent = bitangent;
    }
    else {
        Vector3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * f;
        tangent.Normalize();

        Vector3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * f;
        bitangent.Normalize();

        verts[0].tangent = verts[1].tangent = verts[2].tangent = tangent;
        verts[0].bitangent = verts[1].bitangent = verts[2].bitangent = bitangent;
    }
}

struct Submesh {
    List<gfx::shaders::ModelVertex> vertices;
    List<uint16> indices;
    List<short> textures; // local texture index for each triangle
};

List<Submesh> CreateMesh(d3::Model& model) {
    List<Submesh> submeshes;

    for (int smIndex = 0; auto& submodel : model.submodels) {
        auto& submesh = submeshes.emplace_back();
        int16 index = 0;

        // combine uvs from faces with the vertices
        for (auto& face : submodel.faces) {
            if (face.texNum == -1) continue; // Skip untextured faces as they are metadata such as gunpoints or glows
            // todo: split meshes based on transparency - were the original models designed with this in mind?
            Color color = face.color;

            const auto& fv0 = face.vertices[0];
            const auto& v0 = submodel.vertices[fv0.index];

            auto fvx = &face.vertices[1];
            auto vx = &submodel.vertices[fvx->index];

            // convert triangle fans to triangle lists
            for (int i = 2; i < face.vertices.size(); i++) {
                auto& fv = face.vertices[i];
                auto& v = submodel.vertices[fv.index];
                auto startSize = submesh.vertices.size();

                auto addVert = [&](const d3::Submodel::Vertex& vtx, const Vector2& uv) {
                    color.A(vtx.alpha);

                    submesh.vertices.push_back(gfx::shaders::ModelVertex{
                        .position = vtx.position,
                        .uv = uv,
                        .color = color,
                        .normal = vtx.normal,
                    });
                    submesh.indices.push_back(index++);
                };

                addVert(v0, fv0.uv);
                addVert(*vx, fvx->uv);
                addVert(v, fv.uv);

                PopulateTangents(std::span{ &submesh.vertices[startSize], 3 });

                fvx = &fv;
                vx = &v;

                submesh.textures.push_back(face.texNum);
            }
        }

        smIndex++;
    }

    return submeshes;
}

// todo: must handle d1/d2/d3 and loose resources

enum class LoadedTexHandle : int16 {};

// mapping of texture names to indices in the global texture buffer (descriptor handles)
//      "texture" -> 3
// 
// the index is then assigned to the model texture slots
//      0 -> 3
// and used in the shader to fetch the correct texture
Dictionary<string, LoadedTexHandle> _textureLookup;

List<d3::VClip> _vclips;

struct TextureEntry {
    int entry = -1; // Game table texture entry
    int handle = -1; // loaded texture handle or vclip table (if vclip is set)
    //bool animated; // index into vclips instead of textures
    int vclip = -1; // vclip table index for this entry
    // int vclipHandle = -1; // loaded vclip handle
    // destroyed
};

struct LoadedVClip {
    List<int> handles; // loaded texture handle for each frame
};

List<LoadedVClip> _loadedVClips;

List<TextureEntry> _textures; // maps info for global texture indices

List<gfx::TexHandle> _loadedTextures;

//Option<d3::Bitmap> ReadOutrageBitmap(const d3::Hog2& hog, const d3::GameTable& gameTable, const string& fileName) {
//    for (auto& tex : gameTable.Textures) {
//        string name;
//        if (String::EqualsIgnoreCase(tex.FileName, fileName)) name = fileName;
//        else if (String::EqualsIgnoreCase(tex.FileName, fileName + ".ogf")) name = fileName + ".ogf";
//        else continue;
//
//        if (auto data = hog.ReadEntry(name)) {
//            auto reader = StreamReader(std::move(*data), name);
//            return d3::Bitmap::Read(reader);
//        }
//    }
//
//    return {};
//}

Option<d3::Bitmap> ReadOutrageBitmap(const d3::Hog2& hog, const string& fileName) {
    if (auto data = hog.ReadEntry(fileName)) {
        auto reader = StreamReader(std::move(*data), fileName);
        return d3::Bitmap::Read(reader);
    }

    return {};
}

void ReadVClips(const d3::Hog2& hog, const d3::GameTable& gameTable) {
    for (auto& tex : gameTable.Textures) {
        if (!tex.Animated()) continue;
        if (auto data = hog.ReadEntry(tex.FileName)) {
            auto reader = StreamReader(std::move(*data), tex.FileName);
            auto vclip = d3::VClip::Read(reader);
            vclip.fileName = tex.FileName;
            vclip.frameTime = tex.Speed / vclip.frames.size();
            _vclips.push_back(vclip);
        }
    }
}


string ResolveTextureName(const d3::GameTable& gameTable, const string& fileName) {
    for (auto& tex : gameTable.Textures) {
        if (HasFlag(tex.Flags, d3::TextureFlag::Animated)) {
            for (auto& vclip : _vclips) {
                for (auto& frame : vclip.frames) {
                    if (String::EqualsIgnoreCase(frame.name, fileName))
                        return frame.name;
                    else if (String::EqualsIgnoreCase(frame.name, fileName + ".ogf"))
                        return frame.name;
                }
            }
        }
        else {
            string name;
            if (String::EqualsIgnoreCase(tex.Name, fileName))
                return tex.FileName;
            else if (String::EqualsIgnoreCase(tex.FileName, fileName + ".ogf"))
                return tex.FileName;
        }
    }

    return fileName;
}

//int FindTextureBitmapName(char* name) {
//    int i;
//
//    ASSERT(name != NULL);
//
//    for (i = 0; i < MAX_TEXTURES; i++) {
//        if (GameTextures[i].used) {
//            if (GameTextures[i].flags & TF_ANIMATED) {
//                int not_res = 0;
//                if (GameVClips[GameTextures[i].bm_handle].flags & VCF_NOT_RESIDENT)
//                    not_res = 1;
//
//                PageInVClip(GameTextures[i].bm_handle);
//                vclip* vc = &GameVClips[GameTextures[i].bm_handle];
//                ASSERT(vc->used);
//
//                int t;
//                int retval = -1;
//
//                for (t = 0; t < vc->num_frames && retval == -1; t++)
//                    if ((!stricmp(GameBitmaps[vc->frames[t]].name, name)))
//                        retval = i;
//
//                if (retval != -1)
//                    return retval;
//            }
//            else {
//                if ((!stricmp(GameBitmaps[GameTextures[i].bm_handle].name, name)))
//                    return i;
//            }
//        }
//    }
//
//    return -1;
//}


int FindVClip(string_view name) {
    for (int i = 0; i < _vclips.size(); ++i) {
        if (_vclips[i].fileName == name) return i;
    }

    return -1;
}

bool VClipContainsFrame(const d3::VClip& vclip, string_view name) {
    for (auto& frame : vclip.frames) {
        if (String::EqualsIgnoreCase(frame.name, name)) {
            return true;
        }
        else if (String::EqualsIgnoreCase(frame.name, name + ".ogf")) {
            return true;
        }
    }

    return false;
}

int FindTextureEntry(const d3::GameTable& gameTable, string_view name) {
    for (int i = 0; i < gameTable.Textures.size(); ++i) {
        auto& entry = gameTable.Textures[i];
        if (HasFlag(entry.Flags, d3::TextureFlag::Animated)) {
            auto vclipIndex = FindVClip(entry.FileName);
            if (vclipIndex == -1) continue;

            auto& vclip = _vclips[vclipIndex];
            if (VClipContainsFrame(vclip, name))
                return i;
        }
        else {
            if (String::EqualsIgnoreCase(entry.Name, name)) {
                return i;
            }
            else if (String::EqualsIgnoreCase(entry.FileName, name + ".ogf")) {
                return i;
            }
        }
    }

    return -1;
}


void LoadTextures(const d3::Hog2& hog, const d3::GameTable& gameTable, span<string> textures) {
    // locate textures and upload them to the GPU
    //auto name = ResolveTextureName(gameTable, texture);
    for (auto& texture : textures) {
        auto index = FindTextureEntry(gameTable, texture);

        if (index == -1) {
            SPDLOG_WARN("Unable to find gametable entry for texture `{}`", texture);
            continue;
        }

        auto& entry = gameTable.Textures[index];
        if (HasFlag(entry.Flags, d3::TextureFlag::Animated)) {
            auto vclipIndex = FindVClip(entry.FileName);
            if (vclipIndex == -1) continue;

            auto& vclip = _vclips[vclipIndex];

            if (_textureLookup.contains(entry.FileName)) {
                SPDLOG_INFO("vclip `{}` is already loaded", entry.FileName);
                continue;
            }

            SPDLOG_INFO("Loading vclip {} with {} frames", vclip.fileName, vclip.frames.size());
            _textures.push_back({ index, (int)_loadedVClips.size(), vclipIndex });
            _textureLookup[vclip.fileName] = (LoadedTexHandle)_textures.size();
            auto& loaded = _loadedVClips.emplace_back();

            // Load the individual frames
            for (auto& frame : vclip.frames) {
                if (_textureLookup.contains(frame.name)) {
                    SPDLOG_INFO("texture {} is already loaded", frame.name);
                    continue;
                }

                gfx::Image image;
                image.LoadMipmapped<uint>(frame.mips, frame.width, frame.height);

                loaded.handles.push_back(_loadedTextures.size());

                auto handle = gfx::CreateTexture(image, frame.name);
                _loadedTextures.push_back(handle);
                SPDLOG_INFO("Loaded {}", frame.name);
            }
        }
        else {
            if (_textureLookup.contains(entry.FileName)) {
                SPDLOG_INFO("texture {} is already loaded", entry.FileName);
                continue;
            }

            if (auto data = hog.ReadEntry(entry.FileName)) {
                auto reader = StreamReader(std::move(*data), entry.FileName);
                auto bitmap = d3::Bitmap::Read(reader);

                gfx::Image image;
                image.LoadMipmapped<uint>(bitmap.mips, bitmap.width, bitmap.height);

                auto handle = gfx::CreateTexture(image, entry.FileName);
                //objectTextures[name] = handle;
                _textures.push_back({ index, (int)_loadedTextures.size() });
                _textureLookup[entry.FileName] = (LoadedTexHandle)_loadedTextures.size();
                _loadedTextures.push_back(handle);
                SPDLOG_INFO("Loaded {}", entry.FileName);
            }
        }
    }

    //for (auto& entry : gameTable.Textures) {
    //    if (HasFlag(entry.Flags, d3::TextureFlag::Animated)) {
    //        auto vclipIndex = FindVClip(entry.FileName);
    //        if (vclipIndex == -1) continue;

    //        auto& vclip = vclips[vclipIndex];

    //        for (const auto& texture : textures) {
    //            if (VClipContainsFrame(vclip, texture)) {
    //                SPDLOG_INFO("Loading vclip {} with {} frames", vclip.fileName, vclip.frames.size());
    //                _textures.push_back({ (int)_loadedTextures.size(), vclipIndex });
    //                textureLookup[vclip.fileName] = (LoadedTexHandle)_textures.size();

    //                for (auto& frame : vclip.frames) {
    //                    if (textureLookup.contains(frame.name)) {
    //                        SPDLOG_INFO("texture {} is already loaded", frame.name);
    //                        continue;
    //                    }

    //                    gfx::Image image;
    //                    image.LoadMipmapped<uint>(frame.mips, frame.width, frame.height);

    //                    auto handle = gfx::CreateTexture(image, frame.name);
    //                    _loadedTextures.push_back(handle);
    //                    SPDLOG_INFO("Loaded {}", frame.name);
    //                }
    //                goto next;
    //            }
    //        }

    //        next:;
    //    }
    //    else {
    //        string name;

    //        for (const auto& texture : textures) {
    //            if (String::EqualsIgnoreCase(entry.Name, texture)) {
    //                name = entry.FileName;
    //                break;
    //            }
    //            else if (String::EqualsIgnoreCase(entry.FileName, texture + ".ogf")) {
    //                name = entry.FileName;
    //                break;
    //            }
    //        }

    //        if (name.empty()) continue;

    //        if (textureLookup.contains(name)) {
    //            SPDLOG_INFO("texture {} is already loaded", name);
    //            continue;
    //        }

    //        if (auto data = hog.ReadEntry(name)) {
    //            auto reader = StreamReader(std::move(*data), name);
    //            auto bitmap = d3::Bitmap::Read(reader);

    //            gfx::Image image;
    //            image.LoadMipmapped<uint>(bitmap.mips, bitmap.width, bitmap.height);

    //            auto handle = gfx::CreateTexture(image, name);
    //            //objectTextures[name] = handle;
    //            _textures.push_back({ (int)_loadedTextures.size() });
    //            textureLookup[name] = (LoadedTexHandle)_loadedTextures.size();
    //            _loadedTextures.push_back(handle);
    //            SPDLOG_INFO("Loaded {}", name);
    //        }
    //    }
    //}
}


//Option<Outrage::Bitmap> ReadOutrageBitmap(const string& fileName) {
//    for (auto& tex : GameTable.Textures) {
//        string name;
//        if (String::EqualsIgnoreCase(tex.FileName, fileName)) name = fileName;
//        else if (String::EqualsIgnoreCase(tex.FileName, fileName + ".ogf")) name = fileName + ".ogf";
//        else continue;
//
//        if (auto data = Descent3Hog.ReadEntry(name)) {
//            auto reader = StreamReader(std::move(*data), name);
//            return Outrage::Bitmap::Read(reader);
//        }
//    }
//
//    return {};
//}

// maps the local texture indices to global textures
void MapTextures(const d3::GameTable& gameTable, d3::Model& model) {
    model.textureHandles.resize(model.textures.size());

    for (int i = 0; i < model.textures.size(); ++i) {
        auto& texture = model.textures[i];
        auto name = ResolveTextureName(gameTable, texture);

        //string name;
        //if (String::EqualsIgnoreCase(t.FileName, texture)) name = texture;
        //else if (String::EqualsIgnoreCase(t.FileName, texture + ".ogf")) name = texture + ".ogf";
        //else continue;

        if (auto find = _textureLookup.find(name); find != _textureLookup.end()) {
            model.textureHandles[i] = _loadedTextures[(int)find->second];
            SPDLOG_INFO("Mapping model texture {} to {}", i, (int)find->second);
        }
        else {
            SPDLOG_WARN("Unable to find texture {}", texture);
        }
    }
    //for (auto& texture : model.textures) {
    //    if (auto find = objectTextures.find(texture); find != objectTextures.end()) {
    //            //    }

    //}

    //for (auto& submesh : mesh) {
    //    for (auto& index : submesh.textures) {
    //        if (!Seq::inRange(model.textures, index)) {
    //            SPDLOG_WARN("Model texture index out of range: {}", index);
    //            continue;
    //        }

    //        auto& name = model.textures[index];
    //        // auto find = objectTextures.find(name);
    //        if (auto find = objectTextures.find(name); find != objectTextures.end()) {}
    //    }
    //}
}

void LoadModel(const d3::Hog2& hog, const d3::GameTable& gameTable, string_view name) {
    auto modelData = hog.ReadEntry(name);
    if (!modelData) return;

    StreamReader reader(*modelData);
    auto model = d3::Model::Read(reader);
    SPDLOG_INFO("Read model with {} submodels and {} textures", model.submodels.size(), model.textures.size());

    auto mesh = CreateMesh(model);
    // create mesh and upload

    LoadTextures(hog, gameTable, model.textures);
    MapTextures(gameTable, model);
}

d3::GameTable _gameTable;

void Init() {
    auto hog = d3::Hog2::Read("D:/descent3/d3.hog");


    auto tableData = hog.ReadEntry("Table.gam");
    if (!tableData) return;

    StreamReader tableReader(*tableData);
    _gameTable = d3::GameTable::Read(tableReader);

    ReadVClips(hog, _gameTable);

    LoadModel(hog, _gameTable, "4packConc.OOF");
    LoadModel(hog, _gameTable, "gyro.OOF");
    LoadModel(hog, _gameTable, "kfrog.OOF");


    // todo: pack mesh into buffer. vertices + indices + texture map
    // how to update between frames? 
    // - vertex lighting: ring buffer
    // - level mesh: move to disposal queue + frame index + watch fence value

    // todo: set up shader, bind global texture array, texture buffer, sv_primitive_index

    // camera, frame constants

    //for (auto& [i, smm] : smMeshes) {
    //    auto& mesh = _meshes.emplace_back();
    //    handle.Meshes[smIndex][i] = &mesh;
    //    mesh.VertexBuffer = _buffer.PackVertices(span{ smm.vertices });
    //    mesh.IndexBuffer = _buffer.PackIndices(span{ smm.indices });
    //    mesh.IndexCount = (uint)smm.indices.size();
    //    mesh.Texture = tid;
    //}

    return;
}

void TextureDebugWindow() {
    ImGui::Begin("Textures");

    ImVec2 tileSize = { 128, 128 };
    //switch (Settings::Editor.TexturePreviewSize) {
    //    // case TexturePreviewSize::Small: tileSize = { 48, 48 }; break;
    //    // case TexturePreviewSize::Large: tileSize = { 96, 96 }; break;
    //    default: tileSize = { 64, 64 };
    //}

    //tileSize.x *= Shell::DpiScale;
    //tileSize.y *= Shell::DpiScale;

    //auto cursor = ImGui::GetCursorScreenPos();
    //ImRect tileRect = { cursor, { cursor.x + tileSize.x, cursor.y + tileSize.y} };
    //constexpr int borderThickess = 2;
    constexpr ImVec4 bg = { 0.1f, 0.1f, 0.1f, 1.0f };

    ImGuiStyle& style = ImGui::GetStyle();
    uint i = 0;
    auto count = (uint)_textureLookup.size();
    float contentWidth = ImGui::GetWindowContentRegionMax().x;
    float availableWidth = ImGui::GetWindowPos().x + contentWidth;

    for (auto& entry : _textures) {
        //auto& entry = _textures[(int)index];

        int handle = 0;

        if (entry.vclip == -1) {
            handle = entry.handle;
        }
        else {
            auto& vclip = _vclips[entry.vclip];
            auto frame = vclip.GetFrame(Clock.GetTotalTimeSeconds());
            handle = _loadedVClips[entry.handle].handles[frame];
            //handle = vclips[entry.vclip].frames[0].name;    
        }


        auto texid = _loadedTextures[handle];

        auto& tableEntry = _gameTable.Textures[entry.entry];
        ImGui::ImageButton(tableEntry.Name.c_str(), { texid }, tileSize, { 0, 0 }, { 1, 1 }, bg);

        float spacing = style.ItemSpacing.x / 2.0f;
        float xLast = ImGui::GetItemRectMax().x;
        float xNext = xLast + spacing + tileSize.x; // Expected position if next button was on same line
        if (i + 1 < count && xNext < availableWidth)
            ImGui::SameLine(0, spacing);

        i++;
    }

    ImGui::End();
}

}
