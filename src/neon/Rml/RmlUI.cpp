#include "pch.h"
#include "RmlUI.h"
#include "Graphics/Graphics.h"
#include "Shell.h"
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include "DirectoryWatcher.h"
#include "Graphics/DeviceResources.h"
#include "Graphics/Image.h"
#include "Graphics/UploadBuffer.h"
#include "shaders/rmlui.h"
#include "vfs/FileSystem.h"
#include "vfs/VirtualFileSystem.h"
#include "Graphics/Texture.h"

namespace neon::rml {
    namespace {
        struct FrameResources {
            gfx::GenericBuffer2 geometryBuffer = { 1024 * 4, "rml geometry buffer" };
            //Ptr<gfx::GpuUploadBuffer> uploadBuffer;
            //Ptr<gfx::GpuBuffer> geometryBuffer;
        };

        Ptr<FrameResources> _frameResources;
        Ptr<DirectoryWatcher> _uiFolderWatcher;
        Ptr<DirectoryChangeNotifier> _directoryNotifier;
    }

    struct GeometryHandle {
        intptr_t vertexHandle = {};
        intptr_t indexHandle = {};
        uint indices = 0;
        uint vertices = 0;
    };

    class RmlRenderInterface : public Rml::RenderInterface {
    public:
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
        Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const unsigned char> source, Rml::Vector2i source_dimensions) override;
        void ReleaseTexture(Rml::TextureHandle texture) override;
        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;

        void SetTransform(const Rml::Matrix4f* /*transform*/) override {}

    private:
        bool _enableScissor = false;
        D3D12_RECT _scissor = {};
        List<GeometryHandle> _geometry;
    };

    // Matrix RenderEngine::make_gui_matrix(Rml::Vector2f translation) {
    //    matrix4 mat(1);	//start with identity
    //    dim_t<int> size = { static_cast<int>(currentRenderSize.width), static_cast<int>(currentRenderSize.height) };
    //    mat = Matrix::scale(mat, vector3(1, -1, 1)); //flip
    //    mat = Matrix::scale(mat, vector3(1.0 / (size.width / 2.0), 1.0 / (size.height / 2.0), 1)); //scale into view space
    //    mat = Matrix::translate(mat, vector3(-size.width / 2.0, -size.height / 2.0, 0)); //translate to origin-center
    //    mat = Matrix::translate(mat, vector3(translation.x, translation.y, 0)); //pixel-space offset
    //    return mat;
    //}

    Rml::CompiledGeometryHandle RmlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
        if (!_frameResources) return 0;

        auto index = _geometry.size();
        auto& handle = _geometry.emplace_back();

        /*handle.vertexOffset = _frameResources->vertexBuffer.IncrementalAppend(vertices);
        handle.indexOffset = _frameResources->indexBuffer.IncrementalAppend(indices);*/
        SPDLOG_INFO("compiling {} vertices and {} indices", vertices.size(), indices.size());

        handle.vertexHandle = _frameResources->geometryBuffer.Copy<const Rml::Vertex>(vertices);
        handle.indexHandle = _frameResources->geometryBuffer.Copy<const int>(indices);

        handle.indices = (uint)indices.size();
        handle.vertices = (uint)vertices.size();
        return index + 1; // add 1 because 0 is treated as empty by Rml
    }

    void RmlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
        auto context = gfx::GetGraphicsContext();
        auto cmdList = context->GetCommandList();

        context->SetPipelineState(gfx::pipelines::rmlui);

        auto diffuse = gfx::GetTexture((TexID)texture, true);
        if (diffuse) {
            gfx::shaders::rmlui::SetDiffuse(cmdList, diffuse->GetSRV());
        }
        else {
            gfx::shaders::rmlui::SetDiffuse(cmdList, gfx::GetDeviceResources().whiteTexture->GetSRV());
        }

        gfx::shaders::rmlui::SetSampler(cmdList, gfx::GetDeviceResources().states->LinearClamp());

        auto& handle = _geometry[geometry - 1];

        // Bind shader and vertex buffers
        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = _frameResources->geometryBuffer.GetGPUVirtualAddress(handle.vertexHandle);
        vbv.SizeInBytes = handle.vertices * sizeof(Rml::Vertex);
        vbv.StrideInBytes = sizeof(Rml::Vertex);
        cmdList->IASetVertexBuffers(0, 1, &vbv);

        D3D12_INDEX_BUFFER_VIEW ibv{};
        ibv.BufferLocation = _frameResources->geometryBuffer.GetGPUVirtualAddress(handle.indexHandle);
        ibv.SizeInBytes = handle.indices * sizeof(int);
        ibv.Format = DXGI_FORMAT_R32_UINT;
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        gfx::shaders::rmlui::Arguments args{
            .ProjectionMatrix = DirectX::SimpleMath::Matrix::CreateOrthographicOffCenter(0, (float)shell::width, (float)shell::height, 0, 0.0, -2.0f),
            .translation = { translation.x, translation.y }
        };

        gfx::shaders::rmlui::SetProjectionMatrix(cmdList, args);

        // Set viewport and scissor
        if (_enableScissor) {
            context->SetViewport({ shell::width, shell::height });
            context->SetScissor(_scissor);
        }
        else {
            context->SetViewportAndScissor({ shell::width, shell::height });
        }

        cmdList->DrawIndexedInstanced(handle.indices, 1, 0, 0, 0);
    }

    void RmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
        geometry--; // correct for offset
        SPDLOG_INFO("Release geometry {}", geometry);
        if (geometry >= _geometry.size()) 
            return;
        _frameResources->geometryBuffer.Free(_geometry[geometry].indexHandle);
        _frameResources->geometryBuffer.Free(_geometry[geometry].vertexHandle);
    }

    Rml::TextureHandle RmlRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
        auto data = neon::fs::ReadAllBytes(source);
        if (data.empty()) return 0; // todo: load default texture

        gfx::Image image;
        image.Load<ubyte>(data, texture_dimensions.x, texture_dimensions.y);
        return (int)gfx::UploadTexture(image, "rml texture", true);
    }

    Rml::TextureHandle RmlRenderInterface::GenerateTexture(Rml::Span<const unsigned char> source, Rml::Vector2i source_dimensions) {
        gfx::Image image;
        image.Load(std::span(source.data(), source.size()), source_dimensions.x, source_dimensions.y);
        return (int)gfx::UploadTexture(image, "rml texture", true);
    }

    void RmlRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
        gfx::FreeTexture((TexID)texture);
    }

    void RmlRenderInterface::EnableScissorRegion(bool enable) {
        _enableScissor = enable;
    }

    void RmlRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
        _scissor.left = region.Left();
        _scissor.right = region.Right();
        _scissor.top = region.Top();
        _scissor.bottom = region.Bottom();
    }

    namespace {
        RmlRenderInterface _renderInterface;

        Rml::Context* _context = nullptr;
        Rml::ElementDocument* _document = nullptr;

        //struct ApplicationData {
        //    bool show_text = true;
        //    Rml::String animal = "dog";
        //} _data;

        bool _initialized = false;
    }

    void Init() {
        Rml::SetRenderInterface(&_renderInterface);

        // Now we can initialize RmlUi.
        Rml::Initialise();

        // Create a context to display documents within.
        _context = Rml::CreateContext("main", Rml::Vector2i((int)shell::width, (int)shell::height));

#ifdef _DEBUG
        Rml::Debugger::Initialise(_context);
#endif

        // Tell RmlUi to load the given fonts.
        Rml::LoadFontFace(R"(c:\Windows\Fonts\SegoeUI.ttf)", false);
        Rml::LoadFontFace(R"(c:\Windows\Fonts\seguiemj.ttf)", true); // fallback emoji font


        // Set up data bindings to synchronize application data.
        //if (Rml::DataModelConstructor constructor = _context->CreateDataModel("animals")) {
        //    constructor.Bind("show_text", &_data.show_text);
        //    constructor.Bind("animal", &_data.animal);
        //}

        _context->SetDensityIndependentPixelRatio(shell::dpiScale);

        // Now we are ready to load our document.
        //Rml::ElementDocument* document = _context->LoadDocument("hello_world.rml");
        auto menu = neon::fs::ReadAllText("ui/menu.html");
        _document = _context->LoadDocumentFromMemory(menu);
        if (_document) _document->Show();

        // Replace and style some text in the loaded document.
        //if(auto element = document->GetElementById("message"))
        //    element->SetInnerRML(reinterpret_cast<const char*>(u8"🌍"));

        //element->SetProperty("font-size", "1.5em");

        //_uiFolderWatcher = make_unique<DirectoryWatcher>("ui");
        _directoryNotifier = make_unique<DirectoryChangeNotifier>("ui", 1024);
        _directoryNotifier->AddFileToWatch("menu.html");
        _initialized = true;
    }

    void Shutdown() {
        if (!_initialized) return;
        _uiFolderWatcher.reset();
        _directoryNotifier.reset();
        Rml::Shutdown();
        _frameResources.reset(); // free GPU resources after RML shuts down, as it tries to destroy them too
        
    }

    void Update() {
        if (!_initialized) return;
        //if (my_input->MouseMoved())
        //    _context->ProcessMouseMove(mouse_pos.x, mouse_pos.y, 0);

        for (auto& file : _directoryNotifier->GetChangedFiles()) {
            if (file == "menu.html") {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // hack for file locks
                if (_document) _document->Close();
                SPDLOG_INFO("Reloading: {}", file.string());
                auto menu = neon::fs::ReadAllText("ui/menu.html");
                _document = _context->LoadDocumentFromMemory(menu);
                _document->Show();
            }
        }

        _context->Update();
    }

    void Draw() {
        if (!_initialized) return;

        if (!_frameResources) {
            _frameResources = make_unique<FrameResources>();
            //auto allocator = gfx::GetMemoryAllocator();
        }

        _context->Render();
    }
}
