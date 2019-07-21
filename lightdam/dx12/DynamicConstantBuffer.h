#pragma once

#include "SwapChain.h"
#include <cassert>

// TODO: Introduce a temp constant buffer like this: https://github.com/TheRealMJP/DXRPathTracer/blob/f5825c2da13e237bdc459f80e283a2af38b75826/SampleFramework12/v1.02/Graphics/DX12_Helpers.cpp
// -> Could really use a global frame allocator - would then use that instead of having heaps per constant buffer
// --> This would also eliminate the awkward 'frameIndex' technique I use here everywhere

class DynamicConstantBuffer
{
public:
    DynamicConstantBuffer(const wchar_t* name, ID3D12Device5* device, uint64_t size, uint32_t numBuffers = SwapChain::MaxFramesInFlight);
    ~DynamicConstantBuffer();

    uint8_t* GetData(uint32_t bufferIndex)  { assert(bufferIndex < m_numBuffers); return ((uint8_t*)m_mappedData) + bufferIndex * m_perBufferSize; }
    template<typename T>
    T* GetData(uint32_t bufferIndex)        { assert(sizeof(T) <= m_perBufferSize && bufferIndex < m_numBuffers); return (T*)GetData(bufferIndex); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(uint32_t bufferIndex) const { assert(bufferIndex < m_numBuffers); return m_gpuAddress + bufferIndex * m_perBufferSize; }
    uint32_t GetNumBuffers() const { return m_numBuffers; }

private:
    const uint64_t m_perBufferSize;
    const uint32_t m_numBuffers;
    GraphicsResource m_uploadHeap;
    void* m_mappedData;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress;
};
