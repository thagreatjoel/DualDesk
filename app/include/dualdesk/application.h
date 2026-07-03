#pragma once

#include <windows.h>
#include <memory>
#include <string>

namespace dualdesk {

class DriverInterface;

class Application {
public:
    Application(HINSTANCE hInstance, int argc = 0, wchar_t* argv[] = nullptr);
    ~Application();

    int Run();
    void Shutdown();

private:
    void InitializeModules();
    void CreateMainWindow();
    void Cleanup();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    HINSTANCE instance_handle_ = nullptr;
    HWND main_window_ = nullptr;
    std::unique_ptr<DriverInterface> driverInterface_;
    bool is_running_ = false;
};

} // namespace dualdesk