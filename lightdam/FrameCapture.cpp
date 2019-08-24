#include "FrameCapture.h"
#include "ErrorHandling.h"
#include <fstream>
#include "../external/d3dx12.h"

static bool WritePfm(const float* rgba, uint32_t width, uint32_t height, const std::string& filename)
{
    std::ofstream file(filename.c_str(), std::ios::binary);
    if (!file.bad() && !file.fail())
    {
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
    else
    {
        LogPrint(LogLevel::Failure, "Error writing hdr image to %s", filename.c_str());
        return false;
    }
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
    bufferFootprint.Footprint.RowPitch = (uint32_t)fpRowPitch;
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

void FrameCapture::GetStagingDataAndWriteToPfm(const std::string& filename)
{
    ScopedResourceMap resourceMap(m_stagingResource);
    WritePfm((float*)resourceMap.Get(), m_lastCopiedTextureWidth, m_lastCopiedTextureHeight, filename);
    m_holdsUnsavedCopy = false;
}
