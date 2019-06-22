#include "DynamicConstantBuffer.h"
#include "../MathUtils.h"
#include <string>


DynamicConstantBuffer::DynamicConstantBuffer(const wchar_t* name, ID3D12Device5* device, uint64_t size, int numBuffers)
    : m_perBufferSize(Align<uint64_t>(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
    , m_uploadHeap(GraphicsResource::CreateUploadHeap((std::wstring(L"constant buffer - ") + name).c_str(), m_perBufferSize * numBuffers, device))
    , m_mappedData(m_uploadHeap.Map(0))
    , m_gpuAddress(m_uploadHeap->GetGPUVirtualAddress())
{
}

DynamicConstantBuffer::~DynamicConstantBuffer()
{
    m_uploadHeap.Unmap();
}

uint8_t* GetData(int frameIndex);