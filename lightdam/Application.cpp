#include "Application.h"
#include "Window.h"
#include "dx12/SwapChain.h"
#include "Gui.h"
#include "Scene.h"
#include "PathTracer.h"
#include "ToneMapper.h"
#include "ErrorHandling.h"
#include "FrameCapture.h"

#include <chrono>

#include <dxgi1_6.h>
#include "../external/d3dx12.h"

#include <dxgi1_4.h>
#include <dxgidebug.h>
#include <fstream>

#if defined(_DEBUG)
    #define USE_DEBUG_DEVICE
#endif

Application* Application::s_instance = nullptr;

Application::Application(int argc, char** argv)
    : m_window(new Window(L"LightDam", L"LightDam", 1280, 768))
    , m_shaderDirectoryWatcher(L"shaders")
{
    s_instance = this;

    CreateDeviceAndSwapChain();
    CreateFrameResources();

    m_gui.reset(new Gui(m_window.get(), m_device.Get()));

    unsigned int windowWidth, windowHeight;
    m_window->GetSize(windowWidth, windowHeight);
    m_pathTracer.reset(new PathTracer(m_device.Get(), windowWidth, windowHeight));
    m_toneMapper.reset(new ToneMapper(m_device.Get()));
    m_frameCapture.reset(new FrameCapture());

    m_activeCamera.SetDirection(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
    m_activeCamera.SetPosition(DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
    m_activeCamera.SetUp(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    if (argc > 1)
        LoadScene(argv[1]);
    else
        LoadScene("");

    m_window->AddProcHandler([this](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_SIZE)
        {
            OnWindowResize();
            return true;
        }
        return false;
    });
}

Application::~Application()
{
    s_instance = nullptr;

    m_swapChain->GetGraphicsCommandQueue().WaitUntilAllGPUWorkIsFinished();

    m_commandList = nullptr;
    for (int i = 0; i < SwapChain::MaxFramesInFlight; ++i)
    {
        m_commandAllocators[i]->Reset();
        m_commandAllocators[i] = nullptr;
    }
    

    m_toneMapper.reset();
    m_pathTracer.reset();
    m_scene.reset();
    m_gui.reset();
    m_swapChain.reset();
    m_window.reset();
    m_device = nullptr;

#ifdef USE_DEBUG_DEVICE
    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
#endif
}

void Application::Run()
{
    std::chrono::duration<float> lastFrameTime;
    while (!m_window->IsClosed())
    {
        {
            auto startTime = std::chrono::high_resolution_clock::now();

            m_swapChain->BeginFrame();
            m_window->ProcessWindowMessages();
            m_activeCamera.Update(lastFrameTime.count());
            RenderFrame();

            lastFrameTime = std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - startTime);
        }

        // Shader reload via directory changes.
        if (m_shaderDirectoryWatcher.HasDirectoryFileChangesSinceLastCheck())
        {
            LogPrint(LogLevel::Info, "Reloading shaders ...");
            m_swapChain->GetGraphicsCommandQueue().WaitUntilAllGPUWorkIsFinished();
            m_pathTracer->ReloadShaders();
            m_toneMapper->ReloadShaders();
            LogPrint(LogLevel::Info, "... done reloading shaders");
        }

        // Other scheduled sync operations.
        if (!m_onRenderFinishedCallbacks.empty())
        {
            m_swapChain->GetGraphicsCommandQueue().WaitUntilAllGPUWorkIsFinished();
            for (auto& callback : m_onRenderFinishedCallbacks)
            {
                const size_t numCallbacksBefore = m_onRenderFinishedCallbacks.size();
                callback();
                assert(numCallbacksBefore == m_onRenderFinishedCallbacks.size());
            }
            m_onRenderFinishedCallbacks.clear();
        }
    }
}

void Application::LoadScene(const std::string& pbrtFileName)
{
    std::unique_ptr<Scene> newScene;
    if (pbrtFileName.empty())
        newScene = nullptr; //Scene::LoadTestScene(m_swapChain->GetGraphicsCommandQueue(), m_device.Get());
    else
        newScene = Scene::LoadPbrtScene(pbrtFileName, m_swapChain->GetGraphicsCommandQueue(), m_device.Get());
    if (!newScene)
        return;
    m_scene = std::move(newScene);
    if (!m_scene->GetCameras().empty())
        m_activeCamera = m_scene->GetCameras().front();
    m_pathTracer->SetScene(*m_scene);

    uint32_t screenWidth, screenHeight;
    m_scene->GetScreenSize(screenWidth, screenHeight);
    if (screenWidth && screenHeight)
    {
        m_window->SetSize(screenWidth, screenHeight);
        OnWindowResize();
    }
}

void Application::SaveImage(FrameCapture::FileFormat format)
{
    m_frameCapture->CopyTextureToStaging(m_pathTracer->GetOutputTextureResource(), m_commandList.Get(), m_device.Get());

    WaitForGPUOnNextFrameFinishAndExecute([this, format]()
        {
            std::string screenshotName;
            int i = 0;
            do
            {
                screenshotName = m_scene->GetName() + " (" + std::to_string(m_pathTracer->GetFrameNumber()) + " iterations)." + FrameCapture::s_fileFormatExtensions[(int)format];
            } while (std::ifstream(screenshotName.c_str()));
            m_frameCapture->GetStagingDataAndWriteFile(screenshotName, format);
        });
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
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
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

void Application::OnWindowResize()
{
    uint32_t windowWidth, windowHeight;
    m_window->GetSize(windowWidth, windowHeight);

    // If window got zero size for some reason, don't do the resize on our graphics resources since something is bound to crash with that.
    if (windowWidth == 0 || windowHeight == 0)
        return;

    m_swapChain->Resize(windowWidth, windowHeight);
    m_pathTracer->ResizeOutput(windowWidth, windowHeight);
}

void Application::RenderFrame()
{
    PopulateCommandList();

    // Execute lists and swap.
    m_swapChain->GetGraphicsCommandQueue().ExecuteCommandList(m_commandList.Get());
    m_swapChain->Present();
}

void Application::PopulateCommandList()
{
    const unsigned int frameIndex = m_swapChain->GetCurrentFrameIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    // Set necessary state.
    uint32_t windowWidth, windowHeight;
    m_window->GetSize(windowWidth, windowHeight);
    D3D12_VIEWPORT viewport = D3D12_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight) };
    m_commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = D3D12_RECT{ 0, 0, (LONG)windowWidth, (LONG)windowHeight };
    m_commandList->RSSetScissorRects(1, &scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrentRenderTarget().Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    auto rtvHandle = m_swapChain->GetActiveBackbufferDescriptorHandle();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    if (m_renderingMode == RenderingMode::ProgressiveContinous || m_iterationQueued)
        m_pathTracer->DrawIteration(m_commandList.Get(), m_activeCamera);
    else
        m_pathTracer->SetDescriptorHeap(m_commandList.Get());
    m_iterationQueued = false;
    m_toneMapper->Draw(m_commandList.Get(), m_pathTracer->GetOutputTextureDescHandle());
    m_gui->Draw(*this, m_commandList.Get());

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetCurrentRenderTarget().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}