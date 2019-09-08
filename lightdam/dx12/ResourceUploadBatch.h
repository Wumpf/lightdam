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
    // If targetResource is a texture, this updates only the first subresource (i.e. the highest detailed mipmap)
    void* CreateAndMapUploadResource(GraphicsResource& targetResource, D3D12_RESOURCE_STATES targetResourceStateAfterCopy = D3D12_RESOURCE_STATE_GENERIC_READ);

    // Call this only if you know all copy actions are finished.
    void Clear() { m_uploadBuffers.clear(); }

private:
    void CreateAndMapUploadBuffer(GraphicsResource& targetResource);
    void CreateAndMapUploadTexture(TextureResource& targetResource);

    std::vector<GraphicsResource> m_uploadBuffers;
    ID3D12GraphicsCommandList* m_commandList;
};
