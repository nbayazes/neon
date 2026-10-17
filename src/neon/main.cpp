#include "pch.h"
#define SDL_MAIN_USE_CALLBACKS
#define EASTL_USER_DEFINED_ALLOCATOR 

//#include <RmlUi/Core/Core.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include "Application.h"
#include "Audio/Audio.h"
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

// For EASTL
void* __cdecl operator new[](size_t size, const char* /*name*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/) {
    return new uint8_t[size];
}

void* __cdecl operator new[](unsigned __int64 size, unsigned __int64, unsigned __int64, char const*, int, unsigned int, char const*, int) {
    return new uint8_t[size];
}

namespace {

SDL_Window* _window = nullptr;
//neon::Ptr<SystemInterface_SDL> rmlSystemInterface;
float _cursorCaptureX = 0, _cursorCaptureY = 0;
bool _appFullscreen = false;
bool Fullscreen = false;
neon::uint2 WindowSize;
neon::uint2 WindowPosition;
bool Maximized = false;
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

using neon::uint2;
using neon::uint;

void ClampWindowPosition(uint2& pos, uint2& size) {
    // Check if the saved position is still on screen in case the desktop resolution changes
    auto desktopWidth = (uint)GetSystemMetrics(SM_CXSCREEN);
    auto desktopHeight = (uint)GetSystemMetrics(SM_CYSCREEN);

    if (pos.x >= desktopWidth || pos.y >= desktopHeight)
        pos = { 0, 0 };

    if (size.x <= 640 || size.y <= 480)
        size = uint2(640, 480);
}


void SaveWindowSize() {
    auto hWnd = neon::shell::hwnd;
    if (!hWnd) return;

    WINDOWPLACEMENT placement{ .length = sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(hWnd, &placement);
    Maximized = placement.showCmd == SW_SHOWMAXIMIZED;

    // Only update settings if the window isn't maximized - otherwise it will save garbage
    if (!Maximized) {
        RECT windowRect{};
        GetWindowRect(hWnd, &windowRect);

        // account for borders
        RECT client{};
        GetClientRect(hWnd, &client);

        WindowPosition = { (uint)windowRect.left, (uint)windowRect.top };
        WindowSize = { uint(client.right - client.left), uint(client.bottom - client.top) };
    }
}


// Updates the window fullscreen state based on the app setting
void UpdateFullscreen() {
    auto hWnd = neon::shell::hwnd;
    if (!hWnd) return;
    if (_appFullscreen == Fullscreen) return;

    _appFullscreen = Fullscreen;

    if (!Fullscreen) {
        // Restore window position and size
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

        auto size = WindowSize;
        auto pos = WindowPosition;
        ClampWindowPosition(pos, size);

        // Account for window borders
        RECT windowRect = { 0, 0, (long)size.x, (long)size.y };
        AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, false, 0);
        auto width = windowRect.right - windowRect.left;
        auto height = windowRect.bottom - windowRect.top;

        SetWindowPos(hWnd, HWND_TOP, pos.x, pos.y, width, height, SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(hWnd, Maximized ? SW_SHOWMAXIMIZED : SW_SHOW);

        RECT client{};
        GetClientRect(hWnd, &client);
    }
    else {
        // Windowed fullscreen
        SaveWindowSize();

        SetWindowLongPtr(hWnd, GWL_STYLE, 0);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

        SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    }
}

// called once on startup
SDL_AppResult SDL_AppInit(void** /*appstate*/, int argc, char* argv[]) {
    ConfigureLogging("neon.log");
    SPDLOG_INFO("NEON INIT");

    neon::InitRandom();

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
    neon::audio::Init();

    //rmlSystemInterface = std::make_unique<SystemInterface_SDL>(_window);
    //Rml::SetSystemInterface(rmlSystemInterface.get());
    //neon::rml::Init();

    neon::app::Init();

    return SDL_APP_CONTINUE;
}

// called as frequently as possible
SDL_AppResult SDL_AppIterate(void* /*appstate*/) {
    UpdateFullscreen();

    neon::Clock.Update();
    auto dt = neon::Clock.GetFrameTimeSeconds();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    //ImGui::ShowDemoWindow();

    neon::app::Update(dt);

    ImGui::Render(); // this doesn't actually call any graphics commands, it populates draw data

    neon::app::Render();
    //neon::rml::Update();

    neon::gfx::Present();
    return SDL_APP_CONTINUE;
}

// called on SDL events
SDL_AppResult SDL_AppEvent(void* /*appstate*/, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    auto& io = ImGui::GetIO();

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.scancode == SDL_SCANCODE_F5) {
            neon::gfx::ReloadShaders();
        }

        if (event->key.scancode == SDL_SCANCODE_RETURN && (event->key.mod & SDL_KMOD_ALT)) {
            Fullscreen = !Fullscreen;
        }

        if (!io.WantCaptureKeyboard) {
            neon::app::OnKeyDown(event->key.scancode);
        }
    }

    if (event->type == SDL_EVENT_KEY_UP && !io.WantCaptureKeyboard) {
        neon::app::OnKeyUp(event->key.scancode);
    }

    ImGui_ImplSDL3_ProcessEvent(event);

    // Handle input and window events.
    //running = Backend::ProcessEvents(context, &Shell::ProcessKeyDownShortcuts, true);


    if (!io.WantCaptureMouse) {
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            neon::app::OnMouseMoved(event->motion.x, event->motion.y, event->motion.xrel, event->motion.yrel);
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            neon::app::OnMouseButtonDown(event->button.button);

            if (event->button.button == 1) {
                SDL_SetWindowRelativeMouseMode(_window, true);
                _cursorCaptureX = event->button.x;
                _cursorCaptureY = event->button.y;
            }
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            neon::app::OnMouseButtonUp(event->button.button);

            if (event->button.button == 1) {
                SDL_SetWindowRelativeMouseMode(_window, false);
                SDL_WarpMouseInWindow(_window, _cursorCaptureX, _cursorCaptureY);
            }
        }

        if (event->type == SDL_EVENT_MOUSE_WHEEL) {
            neon::app::OnMouseWheel(event->wheel.x, event->wheel.y);
        }
    }

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
    neon::imgui::Shutdown();
    //neon::rml::Shutdown();
    neon::audio::Shutdown();
    neon::gfx::Shutdown();
}
