#pragma once

#include <windows.h>
#include <memory>

namespace dualdesk {

// Forward declaration
class DriverInterface;

class Application {
public:
    Application(HINSTANCE hInstance, int argc, wchar_t* argv[]);
    ~Application();

    int Run();
    void Shutdown();

private:
    HINSTANCE instance_handle_;
    HWND main_window_ = nullptr;
    bool is_running_ = false;
    
    // Driver interface
    std::unique_ptr<DriverInterface> driverInterface_;  // <-- Add this

    void Initialize();
    void InitializeModules();  // <-- Add this
    void CreateMainWindow();
    void Cleanup();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);
};

} // namespace dualdesk