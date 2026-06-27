#include "dualddesk/application.h"
#include "dualddesk/version.h"
#include "dualddesk/core/logger.h"

#include <string>

namespace dualdesk {

namespace {
    const wchar_t* WINDOW_CLASS_NAME = L"DualDeskMainWindow";
    const wchar_t* WINDOW_TITLE = L"DualDesk";
}

Application::Application(HINSTANCE hInstance, int argc, wchar_t* argv[]) 
    : instance_handle_(hInstance) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    Initialize();
}

Application::~Application() {
    Cleanup();
}

void Application::Initialize() {
    LOG_INFO("DualDesk v{} starting up", Version::GetString());
    CreateMainWindow();
    is_running_ = true;
}

void Application::CreateMainWindow() {
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_handle_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassEx(&wc)) {
        LOG_ERROR("Failed to register window class");
        throw std::runtime_error("Failed to register window class");
    }

    // Create the window (hidden initially)
    main_window_ = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        nullptr,
        nullptr,
        instance_handle_,
        this
    );

    if (!main_window_) {
        LOG_ERROR("Failed to create main window");
        throw std::runtime_error("Failed to create main window");
    }

    LOG_INFO("Main window created: {}", (void*)main_window_);
}

int Application::Run() {
    LOG_INFO("Entering message loop");

    // Show window
    ShowWindow(main_window_, SW_SHOW);
    UpdateWindow(main_window_);

    // Message loop
    MSG msg = {};
    while (is_running_ && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    LOG_INFO("Exiting message loop with code: {}", msg.wParam);
    return static_cast<int>(msg.wParam);
}

void Application::Shutdown() {
    LOG_INFO("Shutdown requested");
    is_running_ = false;
    if (main_window_) {
        PostMessage(main_window_, WM_CLOSE, 0, 0);
    }
}

void Application::Cleanup() {
    LOG_INFO("Cleaning up");
    
    if (main_window_) {
        DestroyWindow(main_window_);
        main_window_ = nullptr;
    }

    UnregisterClass(WINDOW_CLASS_NAME, instance_handle_);
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Application* app = nullptr;

    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCT*>(lparam);
        app = reinterpret_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (app) {
        return app->HandleMessage(msg, wparam, lparam);
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Application::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        LOG_INFO("Window destroyed");
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        LOG_INFO("Window close requested");
        Shutdown();
        return 0;

    case WM_SIZE:
        // Handle window resize
        return 0;

    default:
        return DefWindowProc(main_window_, msg, wparam, lparam);
    }
}

} // namespace dualdesk