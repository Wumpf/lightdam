#pragma once

#include "dx12/SwapChain.h"
#include "dx12/Shader.h"
#include "Camera.h"
#include "DirectoryWatcher.h"
#include "FrameCapture.h"
#include <memory>
#include <string>
#include <chrono>

#include <wrl/client.h>
using namespace Microsoft::WRL;

#include <functional>

class Application
{
public:
    Application(int argc, char** argv);
    ~Application();

    void Run();
    void LoadScene(const std::string& pbrtFileName);
    void SaveImage(FrameCapture::FileFormat format, const char* filename = nullptr);
    
    void DoSamplingIterationInNextFrame() { m_renderIterationQueued = true; }

    using OnRenderFinishedCallback = std::function<void()>;
    void WaitForGPUOnNextFrameFinishAndExecute(const OnRenderFinishedCallback& callback) { m_onRenderFinishedCallbacks.push_back(callback); }

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

    bool m_renderIterationQueued = false;

    std::vector<OnRenderFinishedCallback> m_onRenderFinishedCallbacks;
};
