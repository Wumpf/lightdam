#pragma once

class Window
{
public:
    Window(const wchar_t* windowName, const wchar_t* windowTitle, int width, int height);
    ~Window();

    // Blocking until application exits!
    int ProcessWindowMessages();

private:
    void* m_hwnd;
};

