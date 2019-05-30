#include "Window.h"
#include <Windows.h>

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Window::Window(const wchar_t* windowName, const wchar_t* windowTitle, int width, int height)
{
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = nullptr;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = windowName;
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindowW(
        windowClass.lpszClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // parent
        nullptr,        // menu
        windowClass.hInstance,
        nullptr         // param
    );

    ShowWindow((HWND)m_hwnd, 1);
}

Window::~Window()
{
}

void Window::ProcessWindowMessages()
{
    MSG msg = {};
    while (PeekMessage(&msg, (HWND)m_hwnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT)
        {
            m_closed = true;
            return;
        }
    }
}

void Window::GetSize(unsigned int& width, unsigned int& height) const
{
    RECT rect = {};
    ::GetClientRect((HWND)m_hwnd, &rect);

    // rect.left/top are defined to be zero.
    width = rect.right;
    height = rect.bottom;
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //auto application = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the pointer passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        if (static_cast<UINT8>(wParam) == VK_ESCAPE)
            PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}