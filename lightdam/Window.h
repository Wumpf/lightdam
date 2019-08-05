#pragma once

#include <windef.h>

#include <functional>
#include <map>

class Window
{
public:
    Window(const wchar_t* windowName, const wchar_t* windowTitle, int width, int height);
    ~Window();

    // if true is returned, message was consumed and no other handlers will be called.
    using WndProcHandler = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
    using ProcHandlerHandle = int;

    ProcHandlerHandle AddProcHandler(const WndProcHandler& handler);
    void RemoveProcHandler(ProcHandlerHandle);

    // Processes all messages currently in the message queue.
    void ProcessWindowMessages();

    bool IsClosed() const { return m_closed; }
    HWND GetHandle() const { return m_hwnd; }

    void GetSize(uint32_t& width, uint32_t& height) const;
    void SetSize(uint32_t width, uint32_t height);

private:
    friend LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;

    std::map<ProcHandlerHandle, WndProcHandler> m_procHandlers;
    ProcHandlerHandle m_nextProcHandlerHandle = 0;

    bool m_closed = false;
};

