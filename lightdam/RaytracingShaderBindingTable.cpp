#include "RaytracingShaderBindingTable.h"
#include "MathUtils.h"
#include <algorithm>


RaytracingShaderBindingTable::RaytracingShaderBindingTable(GraphicsResource&& resource, uint32_t rayGenProgramEntrySize, Subtable subtableMiss, Subtable subtableHitGroups)
    : GraphicsResource(std::move(resource))
    , m_rayGenProgramEntrySize(rayGenProgramEntrySize)
    , m_subtableMiss(subtableMiss)
    , m_subtableHitGroups(subtableHitGroups)
{
}

void RaytracingShaderBindingTable::DispatchRays(ID3D12GraphicsCommandList4* commandList, uint32_t width, uint32_t height, uint32_t depth)
{
    auto gpuAddress = Get()->GetGPUVirtualAddress();

    D3D12_DISPATCH_RAYS_DESC desc = {};

    desc.RayGenerationShaderRecord.StartAddress = gpuAddress;
    desc.RayGenerationShaderRecord.SizeInBytes = m_rayGenProgramEntrySize;
    gpuAddress += m_rayGenProgramEntrySize;

    desc.MissShaderTable.StartAddress = gpuAddress;
    desc.MissShaderTable.SizeInBytes = m_subtableMiss.totalSize;
    desc.MissShaderTable.StrideInBytes = m_subtableMiss.stride;
    gpuAddress += m_subtableMiss.totalSize;

    desc.HitGroupTable.StartAddress = gpuAddress;
    desc.HitGroupTable.SizeInBytes = m_subtableHitGroups.totalSize;
    desc.HitGroupTable.StrideInBytes = m_subtableHitGroups.stride;
    gpuAddress += m_subtableHitGroups.totalSize;

    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;

    commandList->DispatchRays(&desc);
}

RaytracingShaderBindingTable RaytracingBindingTableGenerator::Generate(ID3D12StateObjectProperties* raytracingPipelineProperties, ID3D12Device5* device)
{
    auto rayGenProgramEntrySize = m_rayGenProgramEntry.GetSize();
    auto subtableMissSize = m_subtableMissEntries.ComputeTableSize();
    auto subtableHitGroupsSize = m_subtableHitGroupEntries.ComputeTableSize();

    uint32_t bufferSize = rayGenProgramEntrySize + subtableMissSize.totalSize + subtableHitGroupsSize.totalSize;

    GraphicsResource table = GraphicsResource::CreateUploadHeap(L"shader binding table", bufferSize, device);

    auto tableData = ScopedResourceMap(table);
    auto currentRecord = (uint8_t*)tableData.Get();

    m_rayGenProgramEntry.CopyData(raytracingPipelineProperties, currentRecord);
    currentRecord += rayGenProgramEntrySize;

    for (const auto& entry : m_subtableMissEntries.entries)
    {
        entry.CopyData(raytracingPipelineProperties, currentRecord);
        currentRecord += subtableMissSize.stride;
    }
    for (const auto& entry : m_subtableHitGroupEntries.entries)
    {
        entry.CopyData(raytracingPipelineProperties, currentRecord);
        currentRecord += subtableHitGroupsSize.stride;
    }

    assert(currentRecord == (uint8_t*)tableData.Get() + bufferSize);

    return RaytracingShaderBindingTable(std::move(table), rayGenProgramEntrySize, subtableMissSize, subtableHitGroupsSize);
}

void RaytracingBindingTableGenerator::Group::AddEntry(const std::wstring & entryPoint, const std::vector<uint64_t>& inputData)
{
    entries.push_back(Entry{ entryPoint, inputData });
    maxNumParameters = std::max(maxNumParameters, inputData.size());
}

uint32_t RaytracingBindingTableGenerator::Entry::GetSize(size_t numParameters)
{
    return Align<uint32_t>(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8 * (uint32_t)numParameters, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
}

void RaytracingBindingTableGenerator::Entry::CopyData(ID3D12StateObjectProperties* raytracingPipelineProperties, uint8_t* destRecords) const
{
    void* id = raytracingPipelineProperties->GetShaderIdentifier(programEntryPoint.c_str());
    if (!id)
    {
        std::wstring errMsg(std::wstring(L"Unknown shader identifier used in the SBT: ") + programEntryPoint);
        throw errMsg;// std::logic_error(std::string(errMsg.begin(), errMsg.end())); // TODO
    }
    memcpy(destRecords, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    memcpy(destRecords + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, inputData.data(), inputData.size() * 8);
}

RaytracingShaderBindingTable::Subtable RaytracingBindingTableGenerator::Group::ComputeTableSize()
{
    RaytracingShaderBindingTable::Subtable subtable;

    // All entries are made up of an shader identifier and parameters each with 8 bytes.
    // And all need to have the same fixed size!
    subtable.stride = Entry::GetSize(maxNumParameters);
    subtable.totalSize = (uint32_t)(subtable.stride * entries.size());

    return subtable;
}
