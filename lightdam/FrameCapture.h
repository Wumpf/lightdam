#pragma once

#include "dx12/GraphicsResource.h"

#include <cstdint>
#include <string>

class FrameCapture
{
public:
    // Copies a given texture to a internally cahced staging resource.
    void CopyTextureToStaging(const TextureResource& sourceResource, ID3D12GraphicsCommandList* commandList, ID3D12Device* device);

    enum class FileFormat
    {
        Pfm,
        Bmp
    };
    static const char* s_fileFormatExtensions[2];

    // Retrieves data from the staging resource and safes it to file.
    // User needs to ensure copy already "arrived" there.
    void GetStagingDataAndWriteFile(const std::string& filename, FileFormat format);

    bool GetHoldsUnsavedCopy() const { return m_holdsUnsavedCopy; }

private:
    uint32_t m_lastCopiedTextureWidth;
    uint32_t m_lastCopiedTextureHeight;
    GraphicsResource m_stagingResource;
    bool m_holdsUnsavedCopy = false;
};
