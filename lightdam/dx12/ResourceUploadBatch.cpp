#include "ResourceUploadBatch.h"
#include "../../external/d3dx12.h"
#include "../ErrorHandling.h"
#include <assert.h>

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
    assert(targetResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

    m_uploadBuffers.emplace_back(
        (targetResource.GetName() + L"__TempUploadBuffer").c_str(),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, CD3DX12_RESOURCE_DESC::Buffer(targetResource.GetSizeInBytes(), D3D12_RESOURCE_FLAG_NONE),
        GetDevice(targetResource));

    GraphicsResource& tempResource = m_uploadBuffers.back();
    m_commandList->CopyResource(targetResource.Get(), tempResource.Get());
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, targetResourceStateAfterCopy));

    return tempResource.Map();
}

D3D12_SUBRESOURCE_DATA ResourceUploadBatch::CreateAndMapUploadTexture2D(TextureResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy, uint32_t subresourceIndex)
{
    assert(targetResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

    auto device = GetDevice(targetResource);

    uint64_t totalBytes;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint;
    device->GetCopyableFootprints(&targetResource.GetDesc(), subresourceIndex, 1, 0, &bufferFootprint, nullptr, nullptr, &totalBytes); // Note: Guaranteed to be GPU agnostic.

    m_uploadBuffers.emplace_back(
        (targetResource.GetName() + L"__TempUploadTexture").c_str(),
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, CD3DX12_RESOURCE_DESC::Buffer(totalBytes, D3D12_RESOURCE_FLAG_NONE),
            device);

    GraphicsResource& tempResource = m_uploadBuffers.back();
    m_commandList->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(targetResource.Get(), subresourceIndex),
        0, 0, 0,
        &CD3DX12_TEXTURE_COPY_LOCATION(tempResource.Get(), bufferFootprint),
        nullptr);
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, targetResourceStateAfterCopy));

    D3D12_SUBRESOURCE_DATA data;
    data.pData = tempResource.Map();
    data.RowPitch = bufferFootprint.Footprint.RowPitch;
    data.SlicePitch = totalBytes;

    return data;
}
