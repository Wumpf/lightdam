#include "FrameCapture.h"
#include "ErrorHandling.h"
#include "MathUtils.h"
#include <fstream>
#include <algorithm>
#include "../external/d3dx12.h"
#include "../external/stb/stb_image_write.h"

const char* FrameCapture::s_fileFormatExtensions[2] =
{
    "pfm",
    "bmp"
};

static bool WritePfm(const float* rgba, uint32_t width, uint32_t height, const std::string& filename)
{
    std::ofstream file(filename.c_str(), std::ios::binary);
    if (file.bad() || file.fail())
        return false;

    file.write("PF\n", sizeof(char) * 3);
    file << width << " " << height << "\n";
    file.write("-1.000000\n", sizeof(char) * 10);

    for (int y = height - 1; y >= 0; --y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            // We store iteration count in the last channel, need to normalize
            uint32_t index = (y * width + x) * 4;
            float color[3];
            color[0] = rgba[index + 0] / rgba[index + 3];
            color[1] = rgba[index + 1] / rgba[index + 3];
            color[2] = rgba[index + 2] / rgba[index + 3];
            file.write(reinterpret_cast<const char*>(&color), sizeof(float) * 3);
        }
    }
    return true;
}

static bool WriteBmp(const float* rgba, uint32_t width, uint32_t height, const std::string& filename)
{
    std::vector<uint8_t> bmpData(width * height * 3);
    for (int y = height - 1; y >= 0; --y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            // We store iteration count in the last channel, need to normalize
            const float* colorHdr = &rgba[(y * width + x) * 4];
            uint8_t* colorLdr = &bmpData[(y * width + x) * 3];
            colorLdr[0] = (uint8_t)std::min(powf(colorHdr[0] / colorHdr[3], 1.0f / 2.2f) * 255, 255.0f);
            colorLdr[1] = (uint8_t)std::min(powf(colorHdr[1] / colorHdr[3], 1.0f / 2.2f) * 255, 255.0f);
            colorLdr[2] = (uint8_t)std::min(powf(colorHdr[2] / colorHdr[3], 1.0f / 2.2f) * 255, 255.0f);
        }
    }
    return stbi_write_bmp(filename.c_str(), (int)width, (int)height, 3, bmpData.data()) != 0;
}

void FrameCapture::CopyTextureToStaging(const TextureResource& sourceResource, ID3D12GraphicsCommandList* commandList, ID3D12Device* device)
{
    const auto srcDesc = sourceResource->GetDesc();
    m_lastCopiedTextureWidth = sourceResource.GetWidth();
    m_lastCopiedTextureHeight = sourceResource.GetHeight();

    UINT64 totalResourceSize = 0;
    UINT64 fpRowPitch = 0;
    UINT fpRowCount = 0;
    device->GetCopyableFootprints(&srcDesc, 0, 1, 0, nullptr, &fpRowCount, &fpRowPitch, &totalResourceSize);

    if (totalResourceSize > m_stagingResource.GetSizeInBytes())
        m_stagingResource = GraphicsResource::CreateReadbackBuffer(L"FrameCapture staging resource", totalResourceSize, device);
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
    bufferFootprint.Footprint.Width = (uint32_t)sourceResource.GetWidth();
    bufferFootprint.Footprint.Height = (uint32_t)sourceResource.GetHeight();
    bufferFootprint.Footprint.Depth = 1;
    bufferFootprint.Footprint.RowPitch = Align((uint32_t)fpRowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    bufferFootprint.Footprint.Format = sourceResource.GetFormat();

    CD3DX12_TEXTURE_COPY_LOCATION copyDest(m_stagingResource.Get(), bufferFootprint);
    CD3DX12_TEXTURE_COPY_LOCATION copySrc(sourceResource.Get(), 0);

    // some assumptions...
    const auto beforeState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    const auto afterState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(sourceResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(1, &transition);

    commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(sourceResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, afterState);
    commandList->ResourceBarrier(1, &transition);

    m_holdsUnsavedCopy = true;
}

void FrameCapture::GetStagingDataAndWriteFile(const std::string& filename, FileFormat format)
{
    ScopedResourceMap resourceMap(m_stagingResource);
    bool result = false;
    switch (format)
    {
    case FileFormat::Pfm:
        result = WritePfm((float*)resourceMap.Get(), m_lastCopiedTextureWidth, m_lastCopiedTextureHeight, filename);
        break;
    case FileFormat::Bmp:
        result = WriteBmp((float*)resourceMap.Get(), m_lastCopiedTextureWidth, m_lastCopiedTextureHeight, filename);
        break;
    }
    m_holdsUnsavedCopy = false;

    if (result)
        LogPrint(LogLevel::Success, "Wrote screenshot to %s", filename.c_str());
    else
        LogPrint(LogLevel::Failure, "Error writing screenshot to %s", filename.c_str());
}
