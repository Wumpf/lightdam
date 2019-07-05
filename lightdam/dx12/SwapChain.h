#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include "GraphicsResource.h"

using namespace Microsoft::WRL;

class SwapChain
{
public:
    SwapChain(const class Window& window, struct IDXGIFactory4* factory, struct ID3D12Device* device);
    ~SwapChain();

    void Resize(uint32_t width, uint32_t height);

    unsigned int GetCurrentFrameIndex() const               { return m_frameIndex; }
    const TextureResource& GetCurrentRenderTarget() const   { return m_backbuffers[m_bufferIndex]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetActiveBackbufferDescriptorHandle() const;

    ID3D12CommandQueue* GetGraphicsCommandQueue() const  { return m_graphicsCommandQueue.Get(); }

    // Blocks until all gpu work is done on the graphics command queue.
    void WaitUntilGraphicsQueueProcessingDone();

    // * waits until a backbuffer is available for rendering
    // * advances the frame index
    // * ensures frame resources are not still in use by gpu
    void BeginFrame();

    // Calls present and switches the active frame index.
    void Present();

    static const int MaxFramesInFlight = 2;

private:

    void CreateBackbufferResources();

    // As by recommendation of Nvidia and Intel, we have one more buffer than we fill with the gpu.
    // https://developer.nvidia.com/dx12-dos-and-donts
    // https://software.intel.com/en-us/articles/sample-application-for-direct3d-12-flip-model-swap-chains
    // At any point the flip-mode swap chain may look one of its buffers, so we should account for that.
    static const int BufferCount = MaxFramesInFlight + 1;

    ComPtr<ID3D12CommandQueue>      m_graphicsCommandQueue;
    ComPtr<struct IDXGISwapChain3>  m_swapChain;
    TextureResource                 m_backbuffers[BufferCount];
    ComPtr<ID3D12DescriptorHeap>    m_backbufferDescripterHeap;
    unsigned int                    m_rtvDescriptorSize;

    // Synchronization objects.
    unsigned int m_frameIndex;
    unsigned int m_bufferIndex;
    void* m_fenceEvent;
    ComPtr<struct ID3D12Fence> m_fence;
    unsigned long long m_fenceValues[MaxFramesInFlight];
};
