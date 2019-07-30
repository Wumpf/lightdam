#pragma once

#include "dx12/GraphicsResource.h"

#include <cstdint>
#include <string>

class FrameCapture
{
public:
    // Copies a given texture to a internally cahced staging resource.
    void CopyTextureToStaging(const TextureResource& sourceResource, ID3D12GraphicsCommandList* commandList, ID3D12Device* device);

    // Retrieves data from the staging resource and safes it to pfm.
    // User needs to ensure copy already "arrived" there.
    void GetStagingDataAndWriteToPfm(const std::string& filename);

    bool GetHoldsUnsavedCopy() const { return m_holdsUnsavedCopy; }

private:
    uint32_t m_lastCopiedTextureWidth;
    uint32_t m_lastCopiedTextureHeight;
    GraphicsResource m_stagingResource;
    bool m_holdsUnsavedCopy = false;
};
