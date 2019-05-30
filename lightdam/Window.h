#pragma once

class Window
{
public:
    Window(const wchar_t* windowName, const wchar_t* windowTitle, int width, int height);
    ~Window();

    // Processes all messages currently in the message queue.
    void ProcessWindowMessages();

    bool IsClosed() const { return m_closed; }
    void* GetHandle() const { return m_hwnd; }

    void GetSize(unsigned int& width, unsigned int& height) const;

private:
    void* m_hwnd;
    bool m_closed = false;
};

