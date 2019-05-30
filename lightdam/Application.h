#pragma once

#include "SwapChain.h"
#include <memory>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Application
{
public:
    Application(int argc, char** argv);
    ~Application();

    void Run();

private:
    void CreateDeviceAndSwapChain();
    void CreateFrameResources();

    void RenderFrame();
    void PopulateCommandList();


    std::unique_ptr<class Window> m_window;
    std::unique_ptr<class SwapChain> m_swapChain;

    ComPtr<struct ID3D12Device>             m_device;
    
    ComPtr<struct ID3D12CommandAllocator>   m_commandAllocators[SwapChain::FrameCount];
    ComPtr<struct ID3D12GraphicsCommandList> m_commandList;
};
