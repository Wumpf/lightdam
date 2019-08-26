#include "ResourceUploadBatch.h"
#include "../../external/d3dx12.h"

static ID3D12Device* GetDevice(const GraphicsResource& targetResource)
{
    ID3D12Device* device;
    targetResource->GetDevice(IID_PPV_ARGS(&device));
    return device;
}

ResourceUploadBatch::ResourceUploadBatch(ID3D12GraphicsCommandList* commandList)
    : m_commandList(commandList)
{
}

void* ResourceUploadBatch::CreateAndMapUploadBuffer(GraphicsResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy)
{
    m_uploadBuffers.emplace_back(
        (targetResource.GetName() + L"__TempUploadBuffer").c_str(),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, CD3DX12_RESOURCE_DESC::Buffer(targetResource.GetSizeInBytes(), D3D12_RESOURCE_FLAG_NONE),
        GetDevice(targetResource));

    GraphicsResource& tempResource = m_uploadBuffers.back();

    m_commandList->CopyResource(targetResource.Get(), tempResource.Get());
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, targetResourceStateAfterCopy));

    return tempResource.Map();
}
