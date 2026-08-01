#include "pch.h"
#define SDL_MAIN_USE_CALLBACKS

//#include <RmlUi/Core/Core.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include "Application.h"
#include "Graphics/Graphics.h"
#include "imgui_local.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "Logging.h"
#include "neon-types.h"
//#include "Rml/RmlUI.h"
#include "Shell.h"
#include "Rml/RmlUi_Platform_SDL.h"
#include "SystemClock.h"

namespace {

SDL_Window* _window = nullptr;
neon::Ptr<SystemInterface_SDL> rmlSystemInterface;

}

void ParseCommandLine(int argc, char* argv[]) {
    // Skip the first arg, it is the executable path
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "-editor") {}
    }
}

void UpdateWindowSize() {
    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    neon::shell::width = w;
    neon::shell::height = h;
}

// called once on startup
SDL_AppResult SDL_AppInit(void** /*appstate*/, int argc, char* argv[]) {
    ConfigureLogging("neon.log");
    SPDLOG_INFO("NEON INIT");

    if (!SDL_Init(SDL_INIT_VIDEO))
        return SDL_APP_FAILURE;

    ParseCommandLine(argc, argv);
    _window = SDL_CreateWindow("neon framework", 1600, 1200, SDL_WINDOW_RESIZABLE);
    auto properties = SDL_GetWindowProperties(_window);
    auto hwnd = (HWND)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    neon::shell::hwnd = hwnd;

    SDL_SetWindowMinimumSize(_window, 640, 480);

    neon::shell::dpiScale = SDL_GetWindowDisplayScale(_window);
    UpdateWindowSize();

    neon::gfx::DeviceCreationOptions options{
        .enableDebugging = true,
        .allowTearing = true
    };

    neon::imgui::Initialize(_window);
    neon::gfx::Init(hwnd, neon::shell::width, neon::shell::height, options);

    //rmlSystemInterface = std::make_unique<SystemInterface_SDL>(_window);
    //Rml::SetSystemInterface(rmlSystemInterface.get());
    //neon::rml::Init();

    neon::app::Init();

    return SDL_APP_CONTINUE;
}

// called as frequently as possible
SDL_AppResult SDL_AppIterate(void* /*appstate*/) {
    neon::Clock.Update();

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();

    neon::app::TextureDebugWindow();

    ImGui::Render(); // this doesn't actually call any graphics commands, it populates draw data

    //neon::rml::Update();
    neon::app::Update();

    neon::gfx::Present();
    return SDL_APP_CONTINUE;
}

// called on SDL events
SDL_AppResult SDL_AppEvent(void* /*appstate*/, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    ImGui_ImplSDL3_ProcessEvent(event);

    // Handle input and window events.
    //running = Backend::ProcessEvents(context, &Shell::ProcessKeyDownShortcuts, true);


    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        UpdateWindowSize();
        neon::gfx::ScreenSizeChanged(neon::shell::width, neon::shell::height);
    }

    if (event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        neon::shell::dpiScale = SDL_GetWindowDisplayScale(_window);
        // todo: imgui font needs to be recreated if DPI changes
    }

    return SDL_APP_CONTINUE;
}

// Called once on termination
void SDL_AppQuit(void* /*appstate*/, SDL_AppResult /*result*/) {
    SPDLOG_INFO("NEON SHUTDOWN");
    //neon::rml::Shutdown();
    neon::gfx::Shutdown();
}
