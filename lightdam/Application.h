#pragma once

#include "dx12/SwapChain.h"
#include "Camera.h"
#include <memory>
#include <string>

#include <wrl/client.h>
using namespace Microsoft::WRL;

class Application
{
public:
    Application(int argc, char** argv);
    ~Application();

    void Run();
    void LoadScene(const std::string& pbrtFileName);

private:
    void CreateDeviceAndSwapChain();
    void CreateFrameResources();

    void OnWindowResize();

    void RenderFrame();
    void PopulateCommandList();


    std::unique_ptr<class Window> m_window;
    std::unique_ptr<class SwapChain> m_swapChain;
    std::unique_ptr<class Gui> m_gui;
    std::unique_ptr<class Scene> m_scene;
    std::unique_ptr<class PathTracer> m_pathTracer;
    Camera m_activeCamera;

    ComPtr<struct ID3D12CommandAllocator>   m_commandAllocators[SwapChain::MaxFramesInFlight];
    ComPtr<struct ID3D12GraphicsCommandList4> m_commandList;

    ComPtr<struct ID3D12Device5>             m_device;
};
