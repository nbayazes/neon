#include "pch.h"
#include "RmlUI.h"
#include "Graphics/Graphics.h"
#include "Shell.h"
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

namespace neon::rml {
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
    };



    Rml::CompiledGeometryHandle RmlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
        return 0;
    }

    void RmlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
        
    }

    void RmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
        
    }

    Rml::TextureHandle RmlRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
        return 0;

    }

    Rml::TextureHandle RmlRenderInterface::GenerateTexture(Rml::Span<const unsigned char> source, Rml::Vector2i source_dimensions) {
        return 0;
    }

    void RmlRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
        
    }

    void RmlRenderInterface::EnableScissorRegion(bool enable) {
        
    }

    void RmlRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
        
    }

    namespace {
        RmlRenderInterface _renderInterface;

        Rml::Context* _context = nullptr;

        struct ApplicationData {
            bool show_text = true;
            Rml::String animal = "dog";
        } _data;
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
        Rml::LoadFontFace("LatoLatin-Regular.ttf");
        // Fonts can be registered as fallback fonts, as in this case to display emojis.
        Rml::LoadFontFace("NotoEmoji-Regular.ttf", true);

        // Set up data bindings to synchronize application data.
        if (Rml::DataModelConstructor constructor = _context->CreateDataModel("animals"))
        {
            constructor.Bind("show_text", &_data.show_text);
            constructor.Bind("animal", &_data.animal);
        }

        // Now we are ready to load our document.
        Rml::ElementDocument* document = _context->LoadDocument("hello_world.rml");
        document->Show();

        // Replace and style some text in the loaded document.
        Rml::Element* element = document->GetElementById("world");
        element->SetInnerRML(reinterpret_cast<const char*>(u8"🌍"));
        element->SetProperty("font-size", "1.5em");
    }

    void Shutdown() {
        Rml::Shutdown();
    }

    void Update() {
        //if (my_input->MouseMoved())
        //    context->ProcessMouseMove(mouse_pos.x, mouse_pos.y, 0);

        // Update the context to reflect any changes resulting from input events, animations,
        // modified and added elements, or changed data in data bindings.
        _context->Update();

        // Prepare the application for rendering, such as by clearing the window. This calls
        // into the RmlUi backend interface, replace with your own procedures as appropriate.
        //Backend::BeginFrame();

        // Render the user interface. All geometry and other rendering commands are now
        // submitted through the render interface.
        //_context->Render();

        // Present the rendered content, such as by swapping the swapchain. This calls into
        // the RmlUi backend interface, replace with your own procedures as appropriate.
        //Backend::PresentFrame();
    }

    void Render() {
        _context->Render();
    }
}
