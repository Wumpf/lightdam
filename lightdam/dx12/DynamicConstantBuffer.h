#pragma once

#include "SwapChain.h"
#include <cassert>

// TODO: Introduce a temp constant buffer like this: https://github.com/TheRealMJP/DXRPathTracer/blob/f5825c2da13e237bdc459f80e283a2af38b75826/SampleFramework12/v1.02/Graphics/DX12_Helpers.cpp
// -> Could really use a global frame allocator - would then use that instead of having heaps per constant buffer
// --> This would also eliminate the awkward 'frameIndex' technique I use here everywhere

class DynamicConstantBuffer
{
public:
    DynamicConstantBuffer(const wchar_t* name, ID3D12Device5* device, uint64_t size, int numFrames = SwapChain::MaxFramesInFlight);
    ~DynamicConstantBuffer();

    uint8_t* GetData(int frameIndex)                                { return ((uint8_t*)m_mappedData) + frameIndex * m_perBufferSize; }
    template<typename T>
    T* GetData(int frameIndex)                                      { assert(sizeof(T) <= m_perBufferSize); return (T*)GetData(frameIndex); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(int frameIndex) const   { return m_gpuAddress + frameIndex * m_perBufferSize; }

private:
    uint64_t m_perBufferSize;
    GraphicsResource m_uploadHeap;
    void* m_mappedData;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress;
};
