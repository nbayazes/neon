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
#include "Graphics/DeviceResources.h"
#include "Graphics/Graphics.h"
#include "Graphics/Image.h"
#include "Graphics/Mesh.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ModelCache.h"
#include "Scene.h"
#include "ScopedTimer.h"
#include "shaders/Model.h"
#include "shaders/ModelVertex.h"
#include "SystemClock.h"
#include "TextureRegistry.h"

namespace neon {
gfx::Mesh CreateMesh(d3::Model& model);
}

namespace neon::app {

namespace {
    Camera _camera;
    Scene _scene;
    //VClipTable vclipTable;
}


//struct Submesh {
//    List<gfx::shaders::ModelVertex> vertices;
//    List<uint16> indices;
//    List<short> textures; // local texture index for each triangle
//};

// todo: must handle d1/d2/d3 and loose resources

enum class LoadedTexHandle : int16 {};

// mapping of texture names to indices in the global texture buffer (descriptor handles)
//      "texture" -> 3
// 
// the index is then assigned to the model texture slots
//      0 -> 3
// and used in the shader to fetch the correct texture
//CaseInsensitiveDictionary<LoadedTexHandle> _textureLookup;

//List<d3::VClip> _vclips;
//CaseInsensitiveDictionary<string> _vclipFrameLookup; // maps vclip frames to the vclip file name

struct TextureEntry {
    int entry = -1; // Game table texture entry
    int handle = -1; // loaded texture handle 
    int vclipHandle = -1; // vclip table
    //bool animated; // index into vclips instead of textures
    int vclip = -1; // vclip table index for this entry
    // int vclipHandle = -1; // loaded vclip handle
    // destroyed
};

struct LoadedVClip {
    List<int> handles; // loaded texture handle for each frame
};

//List<LoadedVClip> _loadedVClips;

//List<TextureEntry> _textures; // maps info for global texture indices

//List<gfx::TexID> _loadedTextures;

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

            g_VClips.Add(vclip);
            //_vclips.push_back(vclip);

            //for (auto& frame : vclip.frames) {
            //    _vclipFrameLookup[frame.name] = vclip.fileName;
            //}
        }
    }
}

string ResolveTextureName(const d3::GameTable& gameTable, const string& fileName) {
    auto fileNameOgf = fileName + ".ogf";

    for (auto& tex : gameTable.textures) {
        if (HasFlag(tex.flags, d3::TextureFlag::Animated)) {
            if (auto vclip = g_VClips.FindByFrame(fileName))
                return *vclip;

            //if (auto f = _vclipFrameLookup.find(fileName); f != _vclipFrameLookup.end())
            //    return f->second;

            //if (auto f = _vclipFrameLookup.find(fileNameOgf); f != _vclipFrameLookup.end())
            //    return f->second;
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


//int FindVClip(string_view name) {
//    for (int i = 0; i < _vclips.size(); ++i) {
//        if (_vclips[i].fileName == name) return i;
//    }
//
//    return -1;
//}

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
    //auto nameOgf = name + ".ogf";

    //string file;

    //if (auto vclipName = _vclipFrameLookup.find(nameOgf); vclipName != _vclipFrameLookup.end()) {
    //    file = vclipName->second;
    //}
    //else if (vclipName = _vclipFrameLookup.find(name); vclipName != _vclipFrameLookup.end()) {
    //    file = vclipName->second;
    //}

    auto file = g_VClips.FindByFrame(name);

    if (!file) return -1;

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

        if (String::EqualsIgnoreCase(entry.name, name)) {
            return i;
        }
        else if (String::EqualsIgnoreCase(entry.fileName, name + ".ogf")) {
            return i;
        }
    }

    auto vclipFrame = FindVClipFrame(gameTable, name);
    return vclipFrame;
}

//List<gfx::shaders::model::TextureInfo> _textureInfoTable;
//List<char> _textureLoadIndicator;

void LoadTextures(const d3::Hog2& hog, const d3::GameTable& gameTable, span<string> textures) {
    // locate textures and upload them to the GPU

    for (auto& texture : textures) {
        auto index = FindTextureEntry(gameTable, texture);

        if (index == -1) {
            SPDLOG_WARN("Unable to find gametable entry for texture `{}`", texture);
            continue;
        }

        if (g_TextureRegistry.IsLoaded(texture)) continue;


        //if (_textureLoadIndicator[index]) continue;

        //_textureLoadIndicator[index] = true;
        auto& entry = gameTable.textures[index];

        if (HasFlag(entry.flags, d3::TextureFlag::Animated)) {
            auto vclipIndex = g_VClips.FindIndex(entry.fileName);
            if (vclipIndex == -1) continue;

            auto vclip = g_VClips.Get(vclipIndex);
            if (!vclip) continue;

            if (g_TextureRegistry.IsLoaded(vclip->fileName)) {
                SPDLOG_INFO("vclip `{}` is already loaded", entry.fileName);
                continue;
            }

            SPDLOG_INFO("Loading vclip {} with {} frames", vclip->fileName, vclip->frames.size());


            //auto textureInfoIndex = (int)_loadedTextures.size();
            //_textureLookup[vclip->fileName] = (LoadedTexHandle)textureInfoIndex;


            //_textures.push_back({
            //    .entry = index,
            //    .handle = textureInfoIndex,
            //    .vclipHandle = (int)_loadedVClips.size(),
            //    .vclip = vclipIndex
            //});

            //auto& loaded = _loadedVClips.emplace_back();

            List<List<span<const uint>>> textureArray;

            for (auto& frame : vclip->frames) {
                auto& fd = textureArray.emplace_back();

                for (auto& mip : frame.mips) {
                    fd.push_back(mip);
                }

                // todo: resize if frame sizes don't match (shouldn't happen, but it could for user textures)
            }


            gfx::Image image;
            image.LoadArray2D<const uint>(textureArray, vclip->frames[0].width, vclip->frames[0].height);

            //auto handle = gfx::UploadTexture(image, vclip->fileName);
            //loaded.handles.push_back((int)_loadedTextures.size());
            //_loadedTextures.push_back(handle);
            //SPDLOG_INFO("Loaded {} - idx: {}", vclip->fileName, (int)handle);

            //auto& info = _textureInfoTable.emplace_back();
            //info.frames = (int)vclip->frames.size();
            //info.frameTime = vclip->frameTime;
            //info.pingpong = HasFlag(entry.flags, d3::TextureFlag::PingPong);
            //info.index = textureInfoIndex;

            g_TextureRegistry.Upload(vclip->fileName, image, vclipIndex);
        }
        else {
            if (g_TextureRegistry.IsLoaded(entry.fileName)) {
                SPDLOG_INFO("texture {} is already loaded", entry.fileName);
                continue;
            }

            if (auto data = hog.ReadEntry(entry.fileName)) {
                auto reader = StreamReader(std::move(*data), entry.fileName);
                auto bitmap = d3::Bitmap::Read(reader);

                gfx::Image image;
                image.LoadMipmapped<uint>(bitmap.mips, bitmap.width, bitmap.height);
                g_TextureRegistry.Upload(entry.fileName, image);

                //auto handle = gfx::UploadTexture(image, entry.fileName);
                // auto& info = _textureInfoTable.emplace_back();
                // info.index = (int)_loadedTextures.size();
                // _textures.push_back({ index, (int)_loadedTextures.size() });
                // _textureLookup[entry.fileName] = (LoadedTexHandle)_loadedTextures.size();
                // _loadedTextures.push_back(handle);
                //SPDLOG_INFO("Loaded {} - idx: {}", entry.fileName, (uint)handle);
            }
        }
    }


    auto table = g_TextureRegistry.BuildTextureTable(g_VClips.Entries());
    gfx::UpdateTextureInfo(table);
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

        if (auto entry = g_TextureRegistry.Find(name)) {
            model.textureHandles[i] = entry->handle;
            // SPDLOG_INFO("Mapping model texture {} to {}", i, (int)entry->texid);
            SPDLOG_INFO("Mapping model texture {} to {}", model.textures[i], entry->name);
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
auto _meshid = ModelID::None;

d3::Model ReadModel(const d3::Hog2& hog, string_view name) {
    auto modelData = hog.ReadEntry(name);
    if (!modelData) return {};

    StreamReader reader(*modelData);
    auto model = d3::Model::Read(reader);
    SPDLOG_INFO("Read model with {} submodels and {} textures", model.submodels.size(), model.textures.size());
    return model;
}

// todo: superthiefemitter.oof has no geometry at all?
ModelID LoadModel(const d3::Hog2& hog, const d3::GameTable& gameTable, d3::Model& model, string_view name) {
    int64 time = 0;
    ScopedTimer timer(time);

    if (auto existing = g_ModelCache.Find(name); existing != ModelID::None)
        return existing;

    timer.Start();
    LoadTextures(hog, gameTable, model.textures);
    timer.Stop();
    SPDLOG_INFO("Model texture load time: {:.2f} ms", time / 1000.0f);

    timer.Start();
    MapTextures(gameTable, model);
    timer.Stop();
    SPDLOG_INFO("Texture map time: {:.2f} ms", time / 1000.0f);

    timer.Start();
    auto mesh = CreateMesh(model);
    timer.Stop();
    SPDLOG_INFO("Mesh load time: {:.2f} ms", time / 1000.0f);

    //std::array upload = { mesh };
    auto& resources = gfx::GetDeviceResources();
    auto meshId = resources.meshPool->Upload(mesh);
    //gfx::UploadMeshes(upload);

    auto modelId = g_ModelCache.Add(model, name, meshId);
    return modelId;
}

float _cameraDistance = 5;

void Init() {
    auto hog = d3::Hog2::Read("D:/descent3/d3.hog");

    for (auto& entry : hog.entries) {
        if (String::ToLower(entry.name).ends_with(".oof"))
            _modelEntries.push_back(entry);
    }

    auto tableData = hog.ReadEntry("Table.gam");
    if (!tableData) return;


    StreamReader tableReader(*tableData);
    _gameTable = d3::GameTable::Read(tableReader);

    //_textureLoadIndicator.resize(_gameTable.textures.size());

    ReadVClips(hog, _gameTable);

    // auto modelName = "shield.OOF"; 
    auto modelName = "flareyellowbright.oof";
    //auto modelName = "forcefieldswitch.oof";
    //auto modelName = "4packconc.oof";
    auto model = ReadModel(hog, modelName);

    _meshid = LoadModel(hog, _gameTable, model, modelName);
    //mesh.name = modelName;
    _cameraDistance = std::max(model.radius, 2.5f) * 2;

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


void ModelBrowser() {
    ImGui::Begin("Models");

    static int _selection = 0;
    static List<char> _search;
    _search.resize(50);

    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Search", _search.data(), _search.capacity());
    auto searchstr = String::ToLower(string(_search.data()));

    ImGui::BeginChild("models", { -1, -1 }, true);

    for (int i = 0; i < _modelEntries.size(); ++i) {
        auto& entry = _modelEntries[i];

        if (!searchstr.empty()) {
            if (!String::Contains(String::ToLower(entry.name), searchstr))
                continue;
        }

        if (ImGui::Selectable(entry.name.c_str(), i == _selection)) {
            _selection = i;

            auto model = ReadModel(_d3Hog, entry.name);
            //auto mesh = LoadModel(_d3Hog, _gameTable, model, entry.name);
            _meshid = LoadModel(_d3Hog, _gameTable, model, entry.name);
            //mesh.name = entry.name;

            //_meshid++;
            //std::array upload = { mesh };
            //gfx::UploadMeshes(upload);
            //auto bounds = CalculateModelBounds(mesh);
            //auto max = std::max({ bounds.Extents.x, bounds.Extents.y , bounds.Extents.z });
            _cameraDistance = std::max(model.radius, 2.5f) * 3;
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
    auto count = (uint)g_TextureRegistry.Entries().size();
    float contentWidth = ImGui::GetWindowContentRegionMax().x;
    float availableWidth = ImGui::GetWindowPos().x + contentWidth;

    for (auto& entry : g_TextureRegistry.Entries()) {
        //handle = entry.handle;

        //if (entry.vclip == -1) {
        //}
        //else {
        //    if(auto  vclip = g_VClips.Get(entry.vclip)) {
        //        //handle = _loadedVClips[entry.vclipHandle].handles[0];
        //    }
        //    //auto& vclip = vclipTable.Get(entry.vclip);
        //    //auto frame = vclip.GetFrame(Clock.GetTotalTimeSeconds());
        //    
        //    //handle = vclips[entry.vclip].frames[0].name;    
        //}

        ImGui::ImageButton(entry.name.c_str(), { entry.descriptor }, tileSize, { 0, 0 }, { 1, 1 }, bg);

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
    Vector3 dir(5.5, 5.5, 5.5);
    dir.Normalize();

    _camera.Position = dir * _cameraDistance;

    gfx::RenderView(_camera, _meshid);
}

}
