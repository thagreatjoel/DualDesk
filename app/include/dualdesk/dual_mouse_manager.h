// dual_mouse_manager.h
#pragma once

#include <windows.h>
#include <map>
#include <string>
#include <vector>

class DualMouseManager {
public:
    struct MouseCursor {
        POINT position{100, 100};
        COLORREF color = RGB(255, 50, 50);
        std::string name = "Mouse";
        bool active = true;
        int size = 30;
    };

    static DualMouseManager& GetInstance();

    bool Initialize(HWND mainWnd);
    void Shutdown();
    HWND GetOverlayWindow() const;
    void ProcessRawInput(HRAWINPUT lParam);
    void RenderCursors(HDC hdc, RECT rect);
    std::map<HANDLE, MouseCursor>& GetCursors();

private:
    DualMouseManager() = default;
    ~DualMouseManager() = default;
    DualMouseManager(const DualMouseManager&) = delete;
    DualMouseManager& operator=(const DualMouseManager&) = delete;

    HWND m_mainWnd = NULL;
    HWND m_overlayWnd = NULL;
    std::map<HANDLE, MouseCursor> m_cursors;
    int m_colorIndex = 0;
    bool m_running = true;

    COLORREF GetNextColor();
    void RegisterOverlayClass();
    void CreateOverlayWindow();
    void RegisterRawInput();
    void RegisterNewMouse(HANDLE deviceHandle);
    void DrawCursor(HDC hdc, const MouseCursor& cursor);
    void DrawTrail(HDC hdc, const std::vector<POINT>& trail, COLORREF color);

    static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};