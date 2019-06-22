#include "Application.h"
#include "Window.h"
#include "dx12/SwapChain.h"
#include "Gui.h"
#include "Scene.h"
#include "PathTracer.h"
#include "ErrorHandling.h"

#include <dxgi1_6.h>
#include "../external/d3dx12.h"

#include <dxgi1_4.h>
#include <dxgidebug.h>

#if defined(_DEBUG)
    #define USE_DEBUG_DEVICE
#endif

Application::Application(int argc, char** argv)
    : m_window(new Window(L"LightDam", L"LightDam", 1280, 768))
{
    CreateDeviceAndSwapChain();
    CreateFrameResources();

    m_gui.reset(new Gui(m_window.get(), m_device.Get()));
    m_scene = Scene::LoadScene(*m_swapChain, m_device.Get());

    unsigned int windowWidth, windowHeight;
    m_window->GetSize(windowWidth, windowHeight);
    m_pathTracer.reset(new PathTracer(m_device.Get(), windowWidth, windowHeight));
    m_pathTracer->SetScene(*m_scene, m_device.Get());
}

Application::~Application()
{
    m_swapChain->WaitUntilGraphicsQueueProcessingDone();

    m_pathTracer.reset();
    m_scene.reset();
    m_gui.reset();
    m_swapChain.reset();
    m_window.reset();
    for (int i = 0; i < SwapChain::MaxFramesInFlight; ++i)
        m_commandAllocators[i] = nullptr;
    m_commandList = nullptr;
    m_device = nullptr;

#ifdef USE_DEBUG_DEVICE
    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
#endif
}

void Application::Run()
{
    while (!m_window->IsClosed())
    {
        RenderFrame();
        m_window->ProcessWindowMessages();
    }
}

void Application::CreateDeviceAndSwapChain()
{
    UINT dxgiFactoryFlags = 0;

#ifdef USE_DEBUG_DEVICE
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;  // Enable additional debug layers.
        }
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
    }
#endif
    ComPtr<IDXGIFactory5> dxgiFactory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    // Pick first hardward adapter.
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device))))
            break;
        m_device.Reset();
    }

    if (m_device == nullptr)
        throw new std::exception("No graphics device supporting DX12 found!");

    m_swapChain.reset(new SwapChain(*m_window, dxgiFactory.Get(), m_device.Get()));
}

void Application::CreateFrameResources()
{
    // Command allocator.
    for (UINT i = 0; i < SwapChain::MaxFramesInFlight; i++)
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_swapChain->GetCurrentFrameIndex()].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());
}

void Application::RenderFrame()
{
    m_swapChain->BeginFrame();

    PopulateCommandList();

    // Execute lists and swap.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_swapChain->GetGraphicsCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapChain->Present();
}

void Application::PopulateCommandList()
{
    const unsigned int frameIndex = m_swapChain->GetCurrentFrameIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    // Set necessary state.
    unsigned int windowWidth, windowHeight;
    m_window->GetSize(windowWidth, windowHeight);
    //m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    D3D12_VIEWPORT viewport = D3D12_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight) };
    m_commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(windowWidth), static_cast<LONG>(windowHeight) };
    m_commandList->RSSetScissorRects(1, &scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrentRenderTarget().Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    auto rtvHandle = m_swapChain->GetActiveBackbufferDescriptorHandle();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    m_pathTracer->DrawIteration(m_commandList.Get(), m_swapChain->GetCurrentRenderTarget());

    //m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    //m_commandList->DrawInstanced(3, 1, 0, 0);
    m_gui->Draw(m_commandList.Get());

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrentRenderTarget().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}