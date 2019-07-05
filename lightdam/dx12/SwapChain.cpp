#include "SwapChain.h"
#include "../ErrorHandling.h"
#include "../Window.h"

#include <dxgi1_6.h>
#include "../../external/d3dx12.h"

SwapChain::SwapChain(const class Window& window, IDXGIFactory4* factory, ID3D12Device* device)
    : m_frameIndex(0)
    , m_fenceValues{}
{
    // Create synchronization objects.
    ThrowIfFailed(device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
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
    swapChainDesc.BufferCount = BufferCount;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    window.GetSize(swapChainDesc.Width, swapChainDesc.Height);

    // TODO, try out swapchain with tearing.
    // Since we discard frames in present and we always have more than one in flight, this shouldn't add any benefit (other than support for freesync screens etc.)
    // (this is unlike pre-FLIP where tearing was the only way to run at a higher framerate than the screen)
    //BOOL allowTearing = FALSE;
    //auto result = dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));


    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_graphicsCommandQueue.Get(),
        window.GetHandle(),
        &swapChainDesc,
        nullptr, // fullscreen desc
        nullptr, // restrict to output
        swapChain.GetAddressOf()
    ));
    ThrowIfFailed(swapChain.As(&m_swapChain));

    m_swapChain->SetMaximumFrameLatency(MaxFramesInFlight);

    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = SwapChain::BufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_backbufferDescripterHeap)));
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CreateBackbufferResources();
}

SwapChain::~SwapChain()
{
    WaitUntilGraphicsQueueProcessingDone();
    CloseHandle(m_fenceEvent);
}

void SwapChain::Resize(uint32_t width, uint32_t height)
{
    WaitUntilGraphicsQueueProcessingDone();

    for (auto& buffer : m_backbuffers)
        buffer.Release();

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    m_swapChain->GetDesc1(&swapChainDesc);
    m_swapChain->ResizeBuffers(swapChainDesc.BufferCount, width, height, swapChainDesc.Format, swapChainDesc.Flags);
    CreateBackbufferResources();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetActiveBackbufferDescriptorHandle() const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backbufferDescripterHeap->GetCPUDescriptorHandleForHeapStart(), m_bufferIndex, m_rtvDescriptorSize);
    return rtvHandle;
}

void SwapChain::CreateBackbufferResources()
{
    ComPtr<ID3D12Device> device;
    m_swapChain->GetDevice(IID_PPV_ARGS(&device));
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    m_swapChain->GetDesc1(&swapChainDesc);

    for (UINT n = 0; n < BufferCount; n++)
    {
        ID3D12Resource* backbuffer;
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&backbuffer)));
        m_backbuffers[n] = TextureResource(backbuffer, swapChainDesc.Format, swapChainDesc.Width, swapChainDesc.Height, 1);
    }

    // Create a RTV and a command allocator for each frame.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backbufferDescripterHeap->GetCPUDescriptorHandleForHeapStart());
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < SwapChain::BufferCount; i++)
    {
        device->CreateRenderTargetView(m_backbuffers[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    m_bufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void SwapChain::WaitUntilGraphicsQueueProcessingDone()
{
    m_swapChain->GetFrameLatencyWaitableObject();

    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_graphicsCommandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    if (WaitForSingleObject(m_fenceEvent, 1000 * 10) == WAIT_TIMEOUT)
        throw std::exception("Waited for more than 10s on gpu work!");

    // Increment the fence value for the current frame.
    m_frameIndex = (m_frameIndex + 1) % MaxFramesInFlight;
    ++m_fenceValues[m_frameIndex];
}

void SwapChain::BeginFrame()
{
    // Wait for frame latency waitable object.
    if (WaitForSingleObject(m_swapChain->GetFrameLatencyWaitableObject(), 1000 * 10))
        throw std::exception("Waited for more than 10s on gpu work!");

    // Advance frameindex
    m_bufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_frameIndex = (m_frameIndex + 1) % MaxFramesInFlight;

    // If the frame at this slot, is not ready yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        if (WaitForSingleObject(m_fenceEvent, 1000 * 10) == WAIT_TIMEOUT)
            throw std::exception("Waited for more than 10s on gpu work!");
    }

    // Set the fence value for the next frame.
    ++m_fenceValues[m_frameIndex];
}

void SwapChain::Present()
{
    // Sync interval is zero, meaning we use the neweset available buffer and potentially discard others in the queue.
    // This is a FLIP chain, therefore THIS does imply not disabled vsync, DXGI_FEATURE_PRESENT_ALLOW_TEARING does.
    ThrowIfFailed(m_swapChain->Present(0, 0));

    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_graphicsCommandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));
}
