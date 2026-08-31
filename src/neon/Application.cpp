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
#include "vfs/FileSystem.h"

namespace neon {
gfx::Mesh CreateMesh(d3::Model& model, span<d3::TextureFlag> flags);
}

namespace neon::gfx {
void PlayAnimation(const AnimationInstance& animation);
void UpdateAnimations(ModelID modelId, float dt);
}

namespace neon::app {

namespace {
    Camera _camera;
    Scene _scene;
    int _objectSelection = 0;
}

void ReadVClips(const d3::Hog2& hog, const d3::GameTable& gameTable) {
    for (auto& tex : gameTable.textures) {
        if (!HasFlag(tex.flags, d3::TextureFlag::Animated)) continue;
        if (auto data = hog.ReadEntry(tex.fileName)) {
            auto reader = StreamReader(std::move(*data), tex.fileName);
            auto vclip = d3::VClip::Read(reader);
            vclip.fileName = tex.fileName;
            vclip.frameTime = tex.speed / vclip.frames.size();

            g_VClips.Add(vclip);
        }
    }
}

string ResolveTextureName(const d3::GameTable& gameTable, const string& fileName) {
    auto fileNameOgf = fileName + ".ogf";

    for (auto& tex : gameTable.textures) {
        if (HasFlag(tex.flags, d3::TextureFlag::Animated)) {
            if (auto vclip = g_VClips.FindByFrame(fileName))
                return *vclip;
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


int FindVClipFrame(const d3::GameTable& gameTable, const string& name) {
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

int FindGameTableTextureIndex(const d3::GameTable& gameTable, const string& name) {
    for (int i = 0; i < gameTable.textures.size(); ++i) {
        auto& entry = gameTable.textures[i];

        if (String::EqualsIgnoreCase(entry.name, name)) {
            return i;
        }
        else if (String::EqualsIgnoreCase(entry.fileName, name + ".ogf")) {
            return i;
        }
    }

    return FindVClipFrame(gameTable, name);
}

//List<gfx::shaders::model::TextureInfo> _textureInfoTable;
//List<char> _textureLoadIndicator;

void LoadTextures(const d3::Hog2& hog, const d3::GameTable& gameTable, span<string> textures) {
    // locate textures and upload them to the GPU

    for (auto& texture : textures) {
        auto index = FindGameTableTextureIndex(gameTable, texture);

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

            SPDLOG_INFO("Loading vclip {} with {} frames - alpha {}", vclip->fileName, vclip->frames.size(), entry.color.w);

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
            g_TextureRegistry.Upload(vclip->fileName, image, entry.color.w, vclipIndex);
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
                g_TextureRegistry.Upload(entry.fileName, image, entry.color.w);

                SPDLOG_INFO("Loading texture {} - alpha {}", entry.fileName, entry.color.w);
            }
        }
    }

    auto table = g_TextureRegistry.BuildTextureTable(g_VClips.Entries());
    gfx::UpdateTextureInfo(table);
}

void LoadHogTexture(const d3::Hog2& hog, string_view file) {
    if (auto data = hog.ReadEntry(file)) {
        auto reader = StreamReader(std::move(*data));
        auto bitmap = d3::Bitmap::Read(reader);

        gfx::Image image;
        image.LoadMipmapped<uint>(bitmap.mips, bitmap.width, bitmap.height);
        g_TextureRegistry.Upload(file, image);

        SPDLOG_INFO("Loading texture {}", file);
    }
}

// maps the local texture indices to global textures
void MapTextures(const d3::GameTable& gameTable, d3::Model& model) {
    model.textureHandles.resize(model.textures.size());

    for (int i = 0; i < model.textures.size(); ++i) {
        auto& texture = model.textures[i];
        //auto name = ResolveTextureName(gameTable, texture);
        auto textureIndex = FindGameTableTextureIndex(gameTable, texture);

        string registryEntry = texture;

        if (textureIndex >= 0) {
            auto& tableEntry = gameTable.textures[textureIndex];
            registryEntry = tableEntry.fileName;

            // Mark any submodels containing saturated (additive) textures as additive
            //if (HasFlag(tableEntry.flags, d3::TextureFlag::Saturate) || HasFlag(tableEntry.flags, d3::TextureFlag::Alpha)) {
            //    for (auto& submodel : model.submodels) {
            //        for (auto& face : submodel.faces) {
            //            if (face.texNum == i) {
            //                if (HasFlag(tableEntry.flags, d3::TextureFlag::Saturate))
            //                    SetFlag(submodel.flags, d3::SubmodelFlag::Additive);

            //                if (HasFlag(tableEntry.flags, d3::TextureFlag::Alpha))
            //                    SetFlag(submodel.flags, d3::SubmodelFlag::Alpha);
            //                break;
            //            }
            //        }
            //    }
            //}
        }

        if (auto entry = g_TextureRegistry.Find(registryEntry)) {
            model.textureHandles[i] = entry->handle;

            // SPDLOG_INFO("Mapping model texture {} to {}", i, (int)entry->texid);
            SPDLOG_INFO("Mapping model texture {} to {}", model.textures[i], entry->name);
        }
        else {
            SPDLOG_WARN("Unable to find texture {}", texture);
        }
    }
}

d3::GameTable _gameTable;
List<d3::Hog2::Entry> _modelEntries;
d3::Hog2 _d3Hog;
auto _modelId = ModelID::None;


d3::Model ReadModel(const d3::Hog2& hog, span<ubyte> modelData) {
    StreamReader reader(modelData);
    auto model = d3::Model::Read(reader);

    auto glowIndex = (short)model.textures.size();
    bool addedGlow = false;

    for (auto& submodel : model.submodels) {
        if (HasFlag(submodel.flags, d3::SubmodelFlag::Glow)) {
            if (!addedGlow) {
                // hard code the texture for glowing submodels
                model.textures.push_back("thrustball.ogf");
                addedGlow = true;
            }

            for (auto& face : submodel.faces) {
                face.texNum = glowIndex;
            }
        }
    }

    if (addedGlow && !g_TextureRegistry.IsLoaded("thrustball.ogf")) {
        LoadHogTexture(hog, "thrustball.ogf");
    }

    SPDLOG_INFO("Read model with {} submodels and {} textures", model.submodels.size(), model.textures.size());
    return model;
}

List<d3::TextureFlag> GetModelTextureFlags(const d3::GameTable& gameTable, const d3::Model& model) {
    List<d3::TextureFlag> flags(model.textures.size());
    for (int i = 0; i < model.textures.size(); ++i) {
        auto textureIndex = FindGameTableTextureIndex(gameTable, model.textures[i]);
        if (textureIndex == -1) continue;
        auto& tableEntry = gameTable.textures[textureIndex];
        flags[i] = tableEntry.flags;
    }

    return flags;
}

// Splits a model into additional submodels based on transparency
void ExpandTransparentSubmodels(const d3::GameTable& gameTable, d3::Model& model) {
    auto textureFlags = GetModelTextureFlags(gameTable, model);
    List<d3::Submodel> newSubmodels;

    for (auto& submodel : model.submodels) {
        List<d3::ModelFace> transparentFaces;
        List<d3::ModelFace> additiveFaces;

        List<uint> facesToRemove;

        for (int f = 0; f < submodel.faces.size(); ++f) {
            auto& face = submodel.faces[f];
            if (face.texNum < 0) continue;

            auto flags = textureFlags[face.texNum];

            if (HasFlag(flags, d3::TextureFlag::Saturate)) {
                additiveFaces.push_back(face);
                facesToRemove.push_back(f);
            }
            else if (HasFlag(flags, d3::TextureFlag::Alpha)) {
                transparentFaces.push_back(face);
                facesToRemove.push_back(f);
            }
        }

        if (!additiveFaces.empty()) {
            // this doesn't discard the unused vertices / indices as remapping is tedious
            d3::Submodel newSubmodel = submodel;
            newSubmodel.vertices = submodel.vertices;
            newSubmodel.faces = additiveFaces;
            SetFlag(newSubmodel.flags, d3::SubmodelFlag::Additive);
            newSubmodels.push_back(std::move(newSubmodel));
            SPDLOG_INFO("Added new additive submodel");
        }
        else if (!transparentFaces.empty()) {
            // this doesn't discard the unused vertices / indices as remapping is tedious
            d3::Submodel newSubmodel = submodel;
            newSubmodel.vertices = submodel.vertices;
            newSubmodel.faces = transparentFaces;
            SetFlag(newSubmodel.flags, d3::SubmodelFlag::Alpha);
            newSubmodels.push_back(std::move(newSubmodel));
            SPDLOG_INFO("Added new transparent submodel");
        }

        Seq::sortDescending(facesToRemove);
        for (auto& i : facesToRemove) {
            Seq::removeAt(submodel.faces, i);
        }
    }

    for (auto& submodel : newSubmodels) {
        model.submodels.push_back(std::move(submodel));
    }
}

void ExpandAnimationFrames(d3::Model& model) {
    // Rotation keyframes
    for (auto& submodel : model.submodels) {
        std::vector<d3::Submodel::Keyframe> keyframes;

        for (size_t i = 0; i < submodel.keyframes.size(); i++) {
            const auto& frame = submodel.keyframes[i];
            if (frame.frame > keyframes.size() && i > 0) {
                keyframes.push_back(submodel.keyframes[i - 1]);
                SPDLOG_INFO("Duplicating keyframe {}", i - 1);
            }

            keyframes.push_back(frame);
        }

        submodel.keyframes = keyframes;
    }

    // Position keyframes
    for (auto& submodel : model.submodels) {
        std::vector<d3::Submodel::PositionKeyframe> keyframes;

        for (size_t i = 0; i < submodel.positionKeyframes.size(); i++) {
            const auto& frame = submodel.positionKeyframes[i];
            if (frame.frame > keyframes.size() && i > 0) {
                keyframes.push_back(submodel.positionKeyframes[i - 1]);
                SPDLOG_INFO("Duplicating keyframe {}", i - 1);
            }

            keyframes.push_back(frame);
        }

        submodel.positionKeyframes = keyframes;
    }
}

// todo: superthiefemitter.oof has no geometry at all?
ModelID LoadModel(const d3::Hog2& hog, const d3::GameTable& gameTable, d3::Model& model, string_view name) {
    int64 time = 0;
    ScopedTimer timer(time);

    if (auto existing = g_ModelCache.Find(name); existing != ModelID::None)
        return existing;

    //ExpandTransparentSubmodels(gameTable, model);

    //ExpandAnimationFrames(model);

    timer.Start();
    LoadTextures(hog, gameTable, model.textures);
    timer.Stop();
    SPDLOG_INFO("Model texture load time: {:.2f} ms", time / 1000.0f);

    timer.Start();
    MapTextures(gameTable, model);
    timer.Stop();
    SPDLOG_INFO("Texture map time: {:.2f} ms", time / 1000.0f);

    auto textureFlags = GetModelTextureFlags(gameTable, model);

    timer.Start();
    auto mesh = CreateMesh(model, textureFlags);
    timer.Stop();
    SPDLOG_INFO("Mesh load time: {:.2f} ms", time / 1000.0f);

    auto& resources = gfx::GetDeviceResources();
    auto meshId = resources.meshPool->Upload(mesh);

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

    //auto modelName = "shield.OOF"; 
    // auto modelName = "flareyellowbright.oof";
    //auto modelName = "forcefieldswitch.oof";
    //auto modelName = "4packconc.oof";
    //auto modelName = "barnswallow.oof";
    //auto modelName = "fusionblobnewj.oof";
    // auto modelName = "vausstracer.oof";
    // auto modelName = "afterburner2.oof";
    auto modelName = "stinger.oof";
    _objectSelection = 146;
    //auto modelName = "aliencuplinkhousing.oof";

    if (auto modelData = hog.ReadEntry(modelName)) {
        auto model = ReadModel(hog, *modelData);
        _modelId = LoadModel(hog, _gameTable, model, modelName);
        //mesh.name = modelName;
        _cameraDistance = std::max(model.radius, 2.5f) * 2;
    }

    {
        //auto testModelData = fs::ReadAllBytes("D:/dev/froge.oof");
        //auto model = ReadModel(hog, testModelData);

        //auto texture = fs::ReadAllBytes("D:/dev/froge.png");
        //gfx::Image image;
        //if (image.LoadWIC(texture, true))
        //    g_TextureRegistry.Upload("froge", image);

        //_modelId = LoadModel(hog, _gameTable, model, "froge");
        //_cameraDistance = std::max(model.radius, 1.15f) * 2;
    }

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

            if (auto modelData = _d3Hog.ReadEntry(entry.name)) {
                auto model = ReadModel(_d3Hog, *modelData);
                //auto mesh = LoadModel(_d3Hog, _gameTable, model, entry.name);
                _modelId = LoadModel(_d3Hog, _gameTable, model, entry.name);
                //mesh.name = entry.name;

                //_meshid++;
                //std::array upload = { mesh };
                //gfx::UploadMeshes(upload);
                //auto bounds = CalculateModelBounds(mesh);
                //auto max = std::max({ bounds.Extents.x, bounds.Extents.y , bounds.Extents.z });
                _cameraDistance = std::max(model.radius, 2.5f) * 3;
            }
        }
    }

    ImGui::EndChild();

    ImGui::End();
}

enum class MovementType {
    None,
    Physics,
    Walking, // Physics data with different physics pipeline
    AtRest, // Unused or does nothing
    Shockwave, // Concussive forces, weapon shockwaves? Odd that it is here. Comment says it should have been a control type.
    Linked, // Sticky objects that can link to polymodels (such as flares)
};


enum class AnimationState {
    Alert,
    Death,
    Birth,
    Missile1Recoil,
    Missile2,
    Missile2Recoil,
    Melee1,
    Melee1Recoil,
    Melee2,
    Melee2Recoil,
    Idle,
    Quirk,
    Flinch,
    Taunt,
    GotoIdleStanding,
    GotoIdleFlying,
    GotoIdleRolling,
    GotoIdleWalking,
    GotoIdleJumping,
    GotoAlertStanding,
    GotoAlertFlying,
    GotoAlertRolling,
    GotoAlertWalking,
    GotoAlertJumping,
};

constexpr int ANIMATION_TIME_SCALE = 1; // makes animations take longer to play

constexpr const char* MovementTypeLabels[] = {
    "Standing", "Flying", "Rolling", "Walking", "Jumping"
};

constexpr const char* AnimationStateLabels[] = {
    "Alert",
    "Death",
    "Fire Missile 1",
    "Missile Recoil 1",
    "Fire Missile 2",
    "Missile Recoil 2",
    "Melee 1",
    "Melee Recoil 1",
    "Melee 2",
    "Melee Recoil 2",
    "Idle",
    "Quirk",
    "Flinch",
    "Taunt",
    "To Standing Idle",
    "To Flying Idle",
    "To Rolling Idle",
    "To Walking Idle",
    "To Jumping Idle",
    "Goto standing",
    "Goto flying",
    "Goto rolling",
    "Goto walking",
    "Goto jumping"
};

void ObjectBrowser() {
    ImGui::Begin("Object Browser");

    // animation movement type specifies which entry in the anim array to use
    // each sub-entry is a different action (24 total)

    //static int _selection = 0;
    static List<char> _search;
    _search.resize(50);

    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Search", _search.data(), _search.capacity());
    auto searchstr = String::ToLower(string(_search.data()));

    auto height = ImGui::GetWindowHeight();
    ImGui::BeginChild("models", { -1, height - 400 }, true);

    //static d3::GenericInfo selection;

    for (int i = 0; i < _gameTable.objects.size(); ++i) {
        auto& object = _gameTable.objects[i];

        if (object.type != ObjectType::Robot) continue;

        if (!searchstr.empty()) {
            if (!String::Contains(String::ToLower(object.name), searchstr))
                continue;
        }

        if (ImGui::Selectable(object.name.c_str(), i == _objectSelection)) {
            _objectSelection = i;

            if (auto modelData = _d3Hog.ReadEntry(object.modelName)) {
                auto model = ReadModel(_d3Hog, *modelData);
                _modelId = LoadModel(_d3Hog, _gameTable, model, object.name);

                _cameraDistance = std::max(model.radius, 2.5f) * 3;
            }
        }
    }

    ImGui::EndChild();

    {
        ImGui::BeginChild("animations", { -1, -1 });
        auto& object = _gameTable.objects[_objectSelection];
        int16 maxKeyframe = 0;
        static int keyframe = 0;

        for (int m = 0; m < d3::NUM_MOVEMENT_CLASSES; ++m) {
            auto& anim = object.anim.classes[m];
            bool addedHeader = false;

            int id = 0;

            for (int a = 0; a < anim.elems.size(); ++a) {
                auto& action = anim.elems[a];
                if (action.from == 0 && action.to == 0) continue;

                if (!addedHeader) {
                    addedHeader = true;
                    ImGui::Text(MovementTypeLabels[m]);
                }

                ImGui::PushID(100 * m + id++);
                if (ImGui::Button("Play")) {
                    gfx::PlayAnimation({
                        .from = action.from,
                        .to = action.to,
                        .duration = action.speed
                    });
                }
                ImGui::SameLine();
                ImGui::LabelText(AnimationStateLabels[a], "Frames: %i - %i (%.2fs)", action.from, action.to, action.speed);

                ImGui::PopID();


                maxKeyframe = std::max(maxKeyframe, action.to);
            }
        }

        if (maxKeyframe > 0) {
            if (ImGui::SliderInt("Keyframe", &keyframe, 0, maxKeyframe * ANIMATION_TIME_SCALE)) {
                gfx::PlayAnimation({
                    .from = (int16)keyframe,
                    .to = (int16)keyframe,
                    .duration = 0.16f,
                    .timeScale = 1 / (float)ANIMATION_TIME_SCALE
                });
            }
        }

        ImGui::EndChild();
    }

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

void Update(float dt) {
    ModelBrowser();
    ObjectBrowser();
    TextureDebugWindow();
    gfx::UpdateAnimations(_modelId, dt);
}

void Render() {
    Vector3 dir(5.5, 5.5, 5.5);
    dir.Normalize();

    _camera.Position = dir * _cameraDistance;

    gfx::RenderView(_camera, _modelId);
}

}
