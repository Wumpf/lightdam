#include "Application.h"
#include "Window.h"

Application::Application(int argc, char** argv)
    : m_window(new Window(L"LightDam", L"LightDam", 1280, 768))
{
}

Application::~Application()
{
}

int Application::Run()
{
    return m_window->ProcessWindowMessages();
}