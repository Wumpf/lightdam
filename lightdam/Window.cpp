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
        this
    );

    ShowWindow((HWND)m_hwnd, 1);
}

Window::~Window()
{
}

Window::ProcHandlerHandle Window::AddProcHandler(const Window::WndProcHandler& handler)
{
    auto handle = m_nextProcHandlerHandle;
    m_procHandlers.insert(std::make_pair(m_nextProcHandlerHandle, handler));
    ++m_nextProcHandlerHandle;
    return handle;
}

void Window::RemoveProcHandler(Window::ProcHandlerHandle handle)
{
    m_procHandlers.erase(handle);
}

void Window::ProcessWindowMessages()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_closed = true;
            return;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::GetSize(uint32_t& width, uint32_t& height) const
{
    RECT rect = {};
    ::GetClientRect(m_hwnd, &rect);

    // rect.left/top are defined to be zero.
    width = rect.right;
    height = rect.bottom;
}

void Window::SetSize(uint32_t width, uint32_t height)
{
    // via https://stackoverflow.com/a/29007252

    // get size of window and the client area
    RECT rc, rcClient;
    GetWindowRect(m_hwnd, &rc);
    GetClientRect(m_hwnd, &rcClient);

    // calculate size of non-client area
    int xExtra = rc.right - rc.left - rcClient.right;
    int yExtra = rc.bottom - rc.top - rcClient.bottom;

    // now resize based on desired client size
    SetWindowPos(m_hwnd, 0, 0, 0, width + xExtra, height + yExtra, SWP_NOMOVE | SWP_NOOWNERZORDER);
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (window)
    {
        for (const auto handler : window->m_procHandlers)
        {
            if (handler.second(hWnd, message, wParam, lParam))
                return true;
        }
    }

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

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}