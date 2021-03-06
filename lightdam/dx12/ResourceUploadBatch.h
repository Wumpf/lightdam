#pragma once

#include "GraphicsResource.h"
#include <vector>

// Simple upload batch system based on temporary commited resources.
// Inspired by https://github.com/microsoft/DirectXTK12/wiki/ResourceUploadBatch
// (works a bit differently though)
class ResourceUploadBatch
{
public:
    ResourceUploadBatch(ID3D12GraphicsCommandList* commandList);

    // Returns writable data into a temporary upload buffer which is as large as the target resource.
    // Assumes that the target resource is in D3D12_RESOURCE_STATE_COPY_DEST state.
    void* CreateAndMapUploadBuffer(GraphicsResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy = D3D12_RESOURCE_STATE_GENERIC_READ);

    D3D12_SUBRESOURCE_DATA CreateAndMapUploadTexture2D(TextureResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy = D3D12_RESOURCE_STATE_GENERIC_READ, uint32_t subresourceIndex = 0);

    // Call this only if you know all copy actions are finished.
    void Clear() { m_uploadBuffers.clear(); }

private:
    std::vector<GraphicsResource> m_uploadBuffers;
    ID3D12GraphicsCommandList* m_commandList;
};
