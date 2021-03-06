#include "SwapChain.h"
#include "../ErrorHandling.h"
#include "../Window.h"

#include <dxgi1_6.h>
#include "../../external/d3dx12.h"

#include <cassert>

SwapChain::SwapChain(const class Window& window, IDXGIFactory5* factory, ID3D12Device* device)
    : m_graphicsCommandQueue(device, L"SwapChain graphics queue")
    , m_frameIndex(0)
    , m_bufferIndex(0)
    , m_frameExecutionIndices{}
{
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

    // Since we discard frames in present and we always have more than one in flight, this shouldn't add any benefit (other than support for freesync screens etc.)
    // (this is unlike pre-FLIP where tearing was the only way to run at a higher framerate than the screen)
    BOOL allowTearing = FALSE;
    ThrowIfFailed(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));
    if (allowTearing)
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

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
}

void SwapChain::Resize(uint32_t width, uint32_t height)
{
    m_graphicsCommandQueue.WaitUntilAllGPUWorkIsFinished();

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
        ComPtr<ID3D12Resource> backbuffer;
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&backbuffer)));
        m_backbuffers[n] = TextureResource((std::wstring(L"Backbuffer ") + std::to_wstring(n)).c_str(), backbuffer.Get());
    }

    // Create a RTV and a command allocator for each frame.
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    assert(swapChainDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM); // otherwise DXGI_FORMAT_R8G8B8A8_UNORM_SRGB won't work.
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backbufferDescripterHeap->GetCPUDescriptorHandleForHeapStart());
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < SwapChain::BufferCount; i++)
    {
        device->CreateRenderTargetView(m_backbuffers[i].Get(), &rtvDesc, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    m_bufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void SwapChain::BeginFrame()
{
    // Wait for frame latency waitable object.
    if (WaitForSingleObject(m_swapChain->GetFrameLatencyWaitableObject(), 1000 * 10))
        throw std::exception("Waited for more than 10s on gpu work!");

    // Advance frame index.
    m_frameIndex = (m_frameIndex + 1) % MaxFramesInFlight;
    m_bufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the frame at this slot, is not ready yet, wait until it is ready, otherwise we would use resources that are still in use.
    // (latency object might have let us through regardless - it might not even wait for vsync!)
    m_graphicsCommandQueue.WaitUntilExectionIsFinished(m_frameExecutionIndices[m_frameIndex]);
}

void SwapChain::Present()
{
    // Sync interval is zero, meaning we use the neweset available buffer and potentially discard others in the queue.
    // This is a FLIP chain, therefore THIS does imply not disabled vsync, DXGI_FEATURE_PRESENT_ALLOW_TEARING does.
    ThrowIfFailed(m_swapChain->Present(0, 0));

    // Only if m_graphicsCommandQueue.GetLastExecutionIndex is done, this frame slot is available.
    m_frameExecutionIndices[m_frameIndex] = m_graphicsCommandQueue.GetLastExecutionIndex();
}
