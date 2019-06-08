#pragma once

#include "GraphicsResource.h"
#include <d3d12.h>
#include <vector>
#include <memory>


//struct BottomLevelASMesh
//{
//    ID3D12Resource* vertexBuffer;
//    UINT64 vertexOffsetInBytes;     // Offset of the first vertex in the vertex buffer
//    UINT vertexCount;               // Number of vertices to consider in the buffer
//    UINT vertexSizeInBytes;         // Size of a vertex including all its other data, used to stride in the buffer
//
//    ID3D12Resource* transformBuffer; // Buffer with 4x4 transform matrix (reuse per-instance constant buffer!)
//    UINT64  transformOffsetInBytes;  // Offset of the transform matrix in the transform buffer.
//
//    // todo
//    //bool isOpaque;      // If true, the geometry is considered opaque, optimizing the search for a closest hit
//    // index buffer ...
//};

// TODO: Consider expecting index + vertex buffer without offset
// -> enforcing this by design makes it easiert to query vertices in the triangle-hit shaders (something we gonna need to do regardless!)
// -> PBR format we're aiming for doesn't seem to have submaterials (it's always a 'shape' that has a material assigned to)

class BottomLevelAS
{
public:
    static std::unique_ptr<BottomLevelAS> Generate(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& meshes, ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device);

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_blas->GetGPUVirtualAddress(); }

private:
    BottomLevelAS() = default;

    GraphicsResource m_blas;
    GraphicsResource m_scratch; // Can be discarded if we don't plan on rebuilding.
};
