#include "TopLevelAS.h"
#include "BottomLevelAS.h"
#include "MathUtils.h"

using namespace DirectX;


std::unique_ptr<TopLevelAS> TopLevelAS::Generate(const std::vector<BottomLevelASInstance>& blasInstances, ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device)
{
    // Only static TLAS - according to https://devblogs.nvidia.com/rtx-best-practices/ it is better to regenerate TLAS.
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    // Query sizes etc.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc;
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(blasInstances.size());
    prebuildDesc.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    // Create buffers.
    auto tlas = std::unique_ptr<TopLevelAS>(new TopLevelAS());
    tlas->m_tlas = GraphicsResource::CreateBufferForAccellerationStructure(L"TLAS - result", info.ResultDataMaxSizeInBytes, false, device);
    tlas->m_scratch = GraphicsResource::CreateBufferForAccellerationStructure(L"TLAS - scratch", info.ScratchDataSizeInBytes, true, device);
    tlas->m_descriptors = GraphicsResource::CreateUploadHeap(L"TLAS - instance descs", Align<uint64_t>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * blasInstances.size(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT), device);

    // Copy the descriptors in the target descriptor buffer
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = (D3D12_RAYTRACING_INSTANCE_DESC*)tlas->m_descriptors.Map();
    for (int i = 0; i < blasInstances.size(); i++)
    {
        instanceDescs[i].InstanceID = i;    // Instance ID visible in the shader in InstanceID()
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0; // Index of the hit group invoked upon intersection
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        DirectX::XMMATRIX m = XMMatrixTranspose(blasInstances[i].transform); // GLM is column major, the INSTANCE_DESC is row major. TODO NEED THIS??
        memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));
        instanceDescs[i].AccelerationStructure = blasInstances[i].blas->GetGPUAddress();
        instanceDescs[i].InstanceMask = 0xFF; // always visible
    }
    tlas->m_descriptors.Unmap();

    // Create actual TLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = tlas->m_tlas->GetGPUVirtualAddress();
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildDesc.Inputs.Flags = buildFlags;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(blasInstances.size());
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.InstanceDescs = tlas->m_descriptors->GetGPUVirtualAddress();
    buildDesc.SourceAccelerationStructureData = 0;
    buildDesc.ScratchAccelerationStructureData = tlas->m_scratch->GetGPUVirtualAddress();

    // Queue build command & barrier to make use of BLAS.
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = tlas->m_tlas.Get();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);

    return tlas;
}