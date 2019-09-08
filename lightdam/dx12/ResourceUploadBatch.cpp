#include "ResourceUploadBatch.h"
#include "../../external/d3dx12.h"
#include "../ErrorHandling.h"

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

void* ResourceUploadBatch::CreateAndMapUploadResource(GraphicsResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy)
{
    if (targetResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        CreateAndMapUploadBuffer(targetResource);
    else
        CreateAndMapUploadTexture(dynamic_cast<TextureResource&>(targetResource));

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, targetResourceStateAfterCopy));
    return m_uploadBuffers.back().Map();
}

void ResourceUploadBatch::CreateAndMapUploadBuffer(GraphicsResource& targetResource)
{
    m_uploadBuffers.emplace_back(
        (targetResource.GetName() + L"__TempUploadBuffer").c_str(),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, CD3DX12_RESOURCE_DESC::Buffer(targetResource.GetSizeInBytes(), D3D12_RESOURCE_FLAG_NONE),
        GetDevice(targetResource));

    GraphicsResource& tempResource = m_uploadBuffers.back();
    m_commandList->CopyResource(targetResource.Get(), tempResource.Get());
}

void ResourceUploadBatch::CreateAndMapUploadTexture(TextureResource& targetResource)
{
    auto device = GetDevice(targetResource);

    uint32_t numRows;
    uint64_t rowSizeInBytes, totalBytes;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint;
    device->GetCopyableFootprints(&targetResource.GetDesc(), 0, 1, 0, &bufferFootprint, &numRows, &rowSizeInBytes, &totalBytes); // Note: Guaranteed to be GPU agnostic.

    if (totalBytes != targetResource.GetSizeInBytes())
    {
        // This is a normal thing but nothing in our api so far can handle this really...
        LogPrint(LogLevel::Warning,
            "Texture upload buffer is bigger than assumed size of the texture in bytes due to alignment constraints.\nUpload buffer size in bytes: %i, Texture size in bytes %i",
            totalBytes, targetResource.GetSizeInBytes());
    }

    m_uploadBuffers.emplace_back(
        (targetResource.GetName() + L"__TempUploadTexture").c_str(),
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, CD3DX12_RESOURCE_DESC::Buffer(totalBytes, D3D12_RESOURCE_FLAG_NONE),
            device);

    GraphicsResource& tempResource = m_uploadBuffers.back();
    m_commandList->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(targetResource.Get(), 0),
        0, 0, 0,
        &CD3DX12_TEXTURE_COPY_LOCATION(tempResource.Get(), bufferFootprint),
        nullptr);
}
