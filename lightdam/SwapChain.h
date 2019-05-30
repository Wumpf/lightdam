#pragma once

#include <wrl/client.h>
#include <d3d12.h>

using namespace Microsoft::WRL;

class SwapChain
{
public:
    SwapChain(const class Window& window, struct IDXGIFactory4* factory, struct ID3D12Device* device);
    ~SwapChain();

    unsigned int GetCurrentFrameIndex() const { return m_frameIndex; }

    ID3D12Resource* GetRenderTarget(int index) const     { return m_renderTargets[index].Get(); }
    ID3D12Resource* GetCurrentRenderTarget() const       { return m_renderTargets[m_frameIndex].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetDescriptorHandle() const;

    ID3D12CommandQueue* GetGraphicsCommandQueue() const  { return m_graphicsCommandQueue.Get(); }

    // Blocks until all gpu work is done on the graphics command queue.
    void WaitUntilGraphicsQueueProcessingDone();

    // Calls present and waits until 
    void PresentAndSwitchToNextFrame();

    // Two frame buffers, i.e. triple buffering.
    static const int FrameCount = 2;

private:
    ComPtr<ID3D12CommandQueue>   m_graphicsCommandQueue;
    ComPtr<struct IDXGISwapChain3>      m_swapChain;
    ComPtr<ID3D12Resource>       m_renderTargets[FrameCount];
    ComPtr<ID3D12DescriptorHeap> m_backbufferDescripterHeap;
    unsigned int                        m_rtvDescriptorSize;

    // Synchronization objects.
    unsigned int m_frameIndex;
    void* m_fenceEvent;
    ComPtr<struct ID3D12Fence> m_fence;
    unsigned long long m_fenceValues[FrameCount];
};
