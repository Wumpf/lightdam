#include "BottomLevelAS.h"

std::unique_ptr<BottomLevelAS> BottomLevelAS::Generate(const std::vector<BottomLevelASMesh>& meshes, ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device)
{
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> descs(meshes.size());
    for (int i = 0; i < meshes.size(); ++i)
    {
        descs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        descs[i].Flags = meshes[i].isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

        descs[i].Triangles.Transform3x4 = meshes[i].transformBuffer;
        descs[i].Triangles.IndexFormat = meshes[i].indexBuffer ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_UNKNOWN;
        descs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        descs[i].Triangles.IndexCount = meshes[i].indexCount;
        descs[i].Triangles.VertexCount = meshes[i].vertexCount;
        descs[i].Triangles.IndexBuffer = meshes[i].indexBuffer;
        descs[i].Triangles.VertexBuffer = meshes[i].vertexBuffer;
    }

    // Only static BLAS.
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    // Query sizes etc.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc;
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(descs.size());
    prebuildDesc.pGeometryDescs = descs.data();
    prebuildDesc.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    // Create buffers.
    auto blas = std::unique_ptr<BottomLevelAS>(new BottomLevelAS());
    blas->m_scratch = GraphicsResource::CreateBufferForAccellerationStructure(L"BLAS - scratch", info.ScratchDataSizeInBytes, true, device);
    blas->m_blas = GraphicsResource::CreateBufferForAccellerationStructure(L"BLAS - result", info.ResultDataMaxSizeInBytes, false, device);

    // Create actual BLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = blas->m_blas->GetGPUVirtualAddress();
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildDesc.Inputs.Flags = buildFlags;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(descs.size());
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.pGeometryDescs = descs.data();
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.ScratchAccelerationStructureData = blas->m_scratch->GetGPUVirtualAddress();

    // Queue build command & barrier to make use of BLAS.
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = blas->m_blas.Get();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);

    return blas;
}