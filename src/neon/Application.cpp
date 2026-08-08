#include "pch.h"
#include "Application.h"
#include <spdlog/spdlog.h>
#include "ankerl/ankerl.h"
#include "Camera.h"
#include "d3/Hog2.h"
#include "d3/OutrageBitmap.h"
#include "d3/OutrageModel.h"
#include "d3/OutrageTable.h"
#include "Graphics/CommandContext.h"
#include "Graphics/Graphics.h"
#include "Graphics/Image.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Scene.h"
#include "ScopedTimer.h"
#include "shaders/ModelVertex.h"
#include "SystemClock.h"

namespace neon::app {

namespace {
    Camera _camera;
    Scene _scene;
}

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

//struct Submesh {
//    List<gfx::shaders::ModelVertex> vertices;
//    List<uint16> indices;
//    List<short> textures; // local texture index for each triangle
//};

gfx::Mesh CreateMesh(d3::Model& model) {
    gfx::Mesh mesh;
    mesh.model = model;

    for (int smIndex = 0; auto& submodel : model.submodels) {
        auto& submesh = mesh.submeshes.emplace_back();
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

                // Map the local indices to global ones
                submesh.textureIndices.push_back(model.textureHandles[face.texNum]);
                submesh.model = submodel;
            }
        }

        smIndex++;
    }

    return mesh;
}

// todo: must handle d1/d2/d3 and loose resources

enum class LoadedTexHandle : int16 {};

// mapping of texture names to indices in the global texture buffer (descriptor handles)
//      "texture" -> 3
// 
// the index is then assigned to the model texture slots
//      0 -> 3
// and used in the shader to fetch the correct texture
CaseInsensitiveDictionary<LoadedTexHandle> _textureLookup;

List<d3::VClip> _vclips;
CaseInsensitiveDictionary<string> _vclipFrameLookup; // maps vclip frames to the vclip file name

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
    for (auto& tex : gameTable.textures) {
        if (!tex.Animated()) continue;
        if (auto data = hog.ReadEntry(tex.fileName)) {
            auto reader = StreamReader(std::move(*data), tex.fileName);
            auto vclip = d3::VClip::Read(reader);
            vclip.fileName = tex.fileName;
            vclip.frameTime = tex.speed / vclip.frames.size();
            _vclips.push_back(vclip);

            for (auto& frame : vclip.frames) {
                _vclipFrameLookup[frame.name] = vclip.fileName;
            }
        }
    }
}

string ResolveTextureName(const d3::GameTable& gameTable, const string& fileName) {
    auto fileNameOgf = fileName + ".ogf";

    for (auto& tex : gameTable.textures) {
        if (HasFlag(tex.flags, d3::TextureFlag::Animated)) {
            if (auto f = _vclipFrameLookup.find(fileName); f != _vclipFrameLookup.end())
                return f->second;

            if (auto f = _vclipFrameLookup.find(fileNameOgf); f != _vclipFrameLookup.end())
                return f->second;
        }
        else {
            string name;
            if (String::EqualsIgnoreCase(tex.name, fileName))
                return tex.fileName;
            else if (String::EqualsIgnoreCase(tex.fileName, fileNameOgf))
                return tex.fileName;
        }
    }

    return fileName;
}


int FindVClip(string_view name) {
    for (int i = 0; i < _vclips.size(); ++i) {
        if (_vclips[i].fileName == name) return i;
    }

    return -1;
}

//bool VClipContainsFrame(const d3::VClip& vclip, string_view name) {
//    for (auto& frame : vclip.frames) {
//        if (String::EqualsIgnoreCase(frame.name, name)) {
//            return true;
//        }
//        else if (String::EqualsIgnoreCase(frame.name, name + ".ogf")) {
//            return true;
//        }
//    }
//
//    return false;
//}

int FindVClipFrame(const d3::GameTable& gameTable, const string& name) {
    auto nameOgf = name + ".ogf";

    string file;

    if (auto vclipName = _vclipFrameLookup.find(nameOgf); vclipName != _vclipFrameLookup.end()) {
        file = vclipName->second;
    }
    else if (vclipName = _vclipFrameLookup.find(name); vclipName != _vclipFrameLookup.end()) {
        file = vclipName->second;
    }

    if (file.empty()) return -1;

    for (int i = 0; i < gameTable.textures.size(); ++i) {
        auto& entry = gameTable.textures[i];
        if (HasFlag(entry.flags, d3::TextureFlag::Animated) && entry.fileName == file) {
            return i;
        }
    }

    return -1;
}

int FindTextureEntry(const d3::GameTable& gameTable, const string& name) {
    for (int i = 0; i < gameTable.textures.size(); ++i) {
        auto& entry = gameTable.textures[i];
        //if (HasFlag(entry.flags, d3::TextureFlag::Animated)) {
        //    auto vclipIndex = FindVClip(entry.fileName);
        //    if (vclipIndex == -1) continue;

        //    auto& vclip = _vclips[vclipIndex];
        //    if (VClipContainsFrame(vclip, name))
        //        return i;
        //}
        //else {
        if (String::EqualsIgnoreCase(entry.name, name)) {
            return i;
        }
        else if (String::EqualsIgnoreCase(entry.fileName, name + ".ogf")) {
            return i;
        }
        //}
    }

    auto vclipFrame = FindVClipFrame(gameTable, name);
    return vclipFrame;


    //return -1;
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

        auto& entry = gameTable.textures[index];
        if (HasFlag(entry.flags, d3::TextureFlag::Animated)) {
            auto vclipIndex = FindVClip(entry.fileName);
            if (vclipIndex == -1) continue;

            auto& vclip = _vclips[vclipIndex];

            if (_textureLookup.contains(entry.fileName)) {
                SPDLOG_INFO("vclip `{}` is already loaded", entry.fileName);
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

                loaded.handles.push_back((int)_loadedTextures.size());

                auto handle = gfx::CreateTexture(image, frame.name);
                _loadedTextures.push_back(handle);
                SPDLOG_INFO("Loaded {}", frame.name);
            }
        }
        else {
            if (_textureLookup.contains(entry.fileName)) {
                SPDLOG_INFO("texture {} is already loaded", entry.fileName);
                continue;
            }

            if (auto data = hog.ReadEntry(entry.fileName)) {
                auto reader = StreamReader(std::move(*data), entry.fileName);
                auto bitmap = d3::Bitmap::Read(reader);

                gfx::Image image;
                image.LoadMipmapped<uint>(bitmap.mips, bitmap.width, bitmap.height);

                auto handle = gfx::CreateTexture(image, entry.fileName);
                //objectTextures[name] = handle;
                _textures.push_back({ index, (int)_loadedTextures.size() });
                _textureLookup[entry.fileName] = (LoadedTexHandle)_loadedTextures.size();
                _loadedTextures.push_back(handle);
                SPDLOG_INFO("Loaded {}", entry.fileName);
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

        if (auto find = _textureLookup.find(name); find != _textureLookup.end()) {
            model.textureHandles[i] = (int)find->second;
            SPDLOG_INFO("Mapping model texture {} to {}", i, (int)find->second);
        }
        else {
            SPDLOG_WARN("Unable to find texture {}", texture);
        }
    }
}

//struct LoadedMesh {
//    string name;
//    List<gfx::Submesh> mesh;
//};

List<gfx::Mesh> _meshes;
d3::GameTable _gameTable;
List<d3::Hog2::Entry> _modelEntries;
d3::Hog2 _d3Hog;
uint _meshid = -1;

d3::Model ReadModel(const d3::Hog2& hog, string_view name) {
    auto modelData = hog.ReadEntry(name);
    if (!modelData) return {};

    StreamReader reader(*modelData);
    auto model = d3::Model::Read(reader);
    SPDLOG_INFO("Read model with {} submodels and {} textures", model.submodels.size(), model.textures.size());
    return model;
}

gfx::Mesh LoadModel(const d3::Hog2& hog, const d3::GameTable& gameTable, d3::Model& model) {
    int64 time = 0;
    ScopedTimer timer(time);

    timer.Start();
    LoadTextures(hog, gameTable, model.textures);
    timer.Stop();
    SPDLOG_INFO("Model texture load time: {:.2f} ms", time / 1000.0f);

    timer.Start();
    MapTextures(gameTable, model);
    timer.Stop();
    SPDLOG_INFO("Texture map time: {:.2f} ms", time / 1000.0f);

    auto mesh = CreateMesh(model);
    timer.Stop();
    SPDLOG_INFO("Mesh load time: {:.2f} ms", time / 1000.0f);


    return mesh;
}

void Init() {
    auto hog = d3::Hog2::Read("D:/descent3/d3.hog");

    auto tableData = hog.ReadEntry("Table.gam");
    if (!tableData) return;

    for (auto& entry : hog.entries) {
        if (String::ToLower(entry.name).ends_with(".oof"))
            _modelEntries.push_back(entry);
    }


    StreamReader tableReader(*tableData);
    _gameTable = d3::GameTable::Read(tableReader);

    ReadVClips(hog, _gameTable);

    auto model = ReadModel(hog, "shield.OOF");
    auto mesh = LoadModel(hog, _gameTable, model);
    mesh.name = "shield.OOF";

    _meshid = 0;
    std::array upload = { mesh };
    gfx::UploadMeshes(upload);

    _d3Hog = std::move(hog);
}

DirectX::BoundingBox CalculateModelBounds(const gfx::Mesh& mesh) {
    Vector3 min(std::numeric_limits<float>::infinity()), max(-std::numeric_limits<float>::infinity());

    for (auto& submesh : mesh.submeshes) {
        for (auto& v : submesh.vertices) {
            min = Vector3::Min(min, v.position);
            max = Vector3::Max(max, v.position);
        }
    }

    DirectX::BoundingBox bounds;
    DirectX::BoundingBox::CreateFromPoints(bounds, min, max);
    return bounds;
}

float _cameraDistance = 5;

void ModelBrowser() {
    ImGui::Begin("Models");

    ImGui::BeginChild("models", { -1, -1 }, true);

    static int _selection = 0;

    for (int i = 0; i < _modelEntries.size(); ++i) {
        auto& entry = _modelEntries[i];
        if (ImGui::Selectable(entry.name.c_str(), i == _selection)) {
            _selection = i;

            auto model = ReadModel(_d3Hog, entry.name);
            auto mesh = LoadModel(_d3Hog, _gameTable, model);
            mesh.name = entry.name;

            _meshid++;
            std::array upload = { mesh };
            gfx::UploadMeshes(upload);
            //auto bounds = CalculateModelBounds(mesh);
            //auto max = std::max({ bounds.Extents.x, bounds.Extents.y , bounds.Extents.z });
            _cameraDistance = std::max(model.radius, 2.5f) * 2;
        }
    }

    ImGui::EndChild();

    ImGui::End();
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

        auto& tableEntry = _gameTable.textures[entry.entry];
        ImGui::ImageButton(tableEntry.name.c_str(), { texid }, tileSize, { 0, 0 }, { 1, 1 }, bg);

        float spacing = style.ItemSpacing.x / 2.0f;
        float xLast = ImGui::GetItemRectMax().x;
        float xNext = xLast + spacing + tileSize.x; // Expected position if next button was on same line
        if (i + 1 < count && xNext < availableWidth)
            ImGui::SameLine(0, spacing);

        i++;
    }

    ImGui::End();
}

void Update(float /*dt*/) {
    ModelBrowser();
    TextureDebugWindow();
}

void Render() {
    Vector3 dir(5.5, 2.5, 3.5);
    dir.Normalize();

    _camera.Position = dir * _cameraDistance;

    gfx::RenderView(_camera, _meshid);
}

}
