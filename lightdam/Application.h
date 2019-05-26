#pragma once

#include <memory>

class Window;

class Application
{
public:
	Application(int argc, char** argv);
	~Application();

	int Run();

private:
	std::unique_ptr<Window> m_window;
};

