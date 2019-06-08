#include "BottomLevelAS.h"

std::unique_ptr<BottomLevelAS> BottomLevelAS::Generate(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& meshes, ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device)
{
    // Only static BLAS.
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    // Query sizes etc.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc;
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(meshes.size());
    prebuildDesc.pGeometryDescs = meshes.data();
    prebuildDesc.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    // Create buffers.
    auto blas = std::unique_ptr<BottomLevelAS>(new BottomLevelAS());
    blas->m_scratch = GraphicsResource::CreateBufferForAccellerationStructure(info.ScratchDataSizeInBytes, true, device);
    blas->m_blas = GraphicsResource::CreateBufferForAccellerationStructure(info.ResultDataMaxSizeInBytes, false, device);

    // Create actual BLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = blas->m_blas->GetGPUVirtualAddress();
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildDesc.Inputs.Flags = buildFlags;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(meshes.size());
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.pGeometryDescs = meshes.data();
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