#include "SwapChain.h"
#include "DXHelper.h"
#include "ErrorHandling.h"
#include "Window.h"
#include <dxgi1_6.h>

SwapChain::SwapChain(const class Window& window, IDXGIFactory4* factory, ID3D12Device* device)
    : m_frameIndex(0)
    , m_fenceValues{}
{
    // Create synchronization objects.
    ThrowIfFailed(device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[m_frameIndex]++;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Describe and create the graphics queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_graphicsCommandQueue)));

    // Create swapchain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    window.GetSize(swapChainDesc.Width, swapChainDesc.Height);

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_graphicsCommandQueue.Get(),
        (HWND)window.GetHandle(),
        &swapChainDesc,
        nullptr, // fullscreen desc
        nullptr, // restrict to output
        swapChain.GetAddressOf()
    ));
    ThrowIfFailed(swapChain.As(&m_swapChain));

    for (UINT n = 0; n < FrameCount; n++)
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_backbuffers[n])));


    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = SwapChain::FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_backbufferDescripterHeap)));
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create a RTV and a command allocator for each frame.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backbufferDescripterHeap->GetCPUDescriptorHandleForHeapStart());
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < SwapChain::FrameCount; i++)
    {
        device->CreateRenderTargetView(GetBackbuffer(i), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }
}

SwapChain::~SwapChain()
{
    WaitUntilGraphicsQueueProcessingDone();
    CloseHandle(m_fenceEvent);
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetActiveBackbufferDescriptorHandle() const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backbufferDescripterHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    return rtvHandle;
}

void SwapChain::WaitUntilGraphicsQueueProcessingDone()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_graphicsCommandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    if (WaitForSingleObject(m_fenceEvent, 1000 * 10) == WAIT_TIMEOUT)
        throw std::exception("Waited for more than 10s on gpu work!");

    // Increment the fence value for the current frame.
    m_fenceValues[m_frameIndex]++;
}

void SwapChain::PresentAndSwitchToNextFrame()
{
    ThrowIfFailed(m_swapChain->Present(1, 0));

    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_graphicsCommandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        if (WaitForSingleObject(m_fenceEvent, 1000 * 10) == WAIT_TIMEOUT)
            throw std::exception("Waited for more than 10s on gpu work!");
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}
