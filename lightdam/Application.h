#pragma once

#include "dx12/SwapChain.h"
#include "dx12/Shader.h"
#include "Camera.h"
#include "DirectoryWatcher.h"
#include <memory>
#include <string>

#include <wrl/client.h>
using namespace Microsoft::WRL;

class Application
{
public:
    enum class RenderingMode
    {
        ProgressiveContinous,
        ProgressiveManual,
    };

    Application(int argc, char** argv);
    ~Application();

    void Run();
    void LoadScene(const std::string& pbrtFileName);
    void SaveHdrImage();

    const class Scene& GetScene() const          { return *m_scene; }
    class PathTracer& GetPathTracer()            { return *m_pathTracer; }
    class ControllableCamera& GetActiveCamera()  { return m_activeCamera; }
    
    RenderingMode GetRenderingMode() const       { return m_renderingMode; }
    void SetRenderingMode(RenderingMode newMode) { m_renderingMode = newMode; }
    void QueueSingleRenderIteration()            { m_iterationQueued = true; }

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
    std::unique_ptr<class ToneMapper> m_toneMapper;
    std::unique_ptr<class FrameCapture> m_frameCapture;
    ControllableCamera m_activeCamera;
    DirectoryWatcher m_shaderDirectoryWatcher;

    ComPtr<struct ID3D12CommandAllocator>   m_commandAllocators[SwapChain::MaxFramesInFlight];
    ComPtr<struct ID3D12GraphicsCommandList4> m_commandList;

    ComPtr<struct ID3D12Device5>             m_device;

    RenderingMode m_renderingMode = RenderingMode::ProgressiveContinous;
    bool m_iterationQueued = false;
};
