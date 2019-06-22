#pragma once

#include "GraphicsResource.h"
#include <d3d12.h>
#include <vector>
#include <memory>


struct BottomLevelASMesh
{
    D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE vertexBuffer;
    uint32_t vertexCount;               // Number of vertices to consider in the buffer

    D3D12_GPU_VIRTUAL_ADDRESS indexBuffer = 0; // 32bit index buffer
    uint32_t indexCount = 0;

    D3D12_GPU_VIRTUAL_ADDRESS transformBuffer = 0; // (optional) buffer with 3x4 transform matrix (reuse per-instance constant buffer!)

    bool isOpaque = true;
};

class BottomLevelAS
{
public:
    static std::unique_ptr<BottomLevelAS> Generate(const std::vector<BottomLevelASMesh>& meshes, ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device);

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_blas->GetGPUVirtualAddress(); }

private:
    BottomLevelAS() = default;

    GraphicsResource m_blas;
    GraphicsResource m_scratch; // Can be discarded if we don't plan on rebuilding.
};
