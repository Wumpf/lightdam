#include "RaytracingShaderBindingTable.h"
#include "../MathUtils.h"
#include <algorithm>

void RaytracingShaderBindingTable::DispatchRays(ID3D12GraphicsCommandList4* commandList, uint32_t width, uint32_t height, uint32_t depth)
{
    D3D12_DISPATCH_RAYS_DESC desc = {};

    desc.RayGenerationShaderRecord = m_rayGenerationShaderRecord;
    desc.MissShaderTable = m_missShaderTable;
    desc.HitGroupTable = m_hitGroupTable;

    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;

    commandList->DispatchRays(&desc);
}

RaytracingShaderBindingTable RaytracingBindingTableGenerator::Generate(ID3D12StateObjectProperties* raytracingPipelineProperties, ID3D12Device5* device)
{
    RaytracingShaderBindingTable table;

    // Startaddresses of subtables must be D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
    // For simplicity we just round up on the subtable sizes which is pretty much only a conceptual difference.

    table.m_rayGenerationShaderRecord.SizeInBytes = Align<uint64_t>(m_rayGenProgramEntry.GetSize(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    table.m_missShaderTable.StrideInBytes = Entry::GetSize(m_subtableMissEntries.maxNumParameters);
    table.m_missShaderTable.SizeInBytes = Align(table.m_missShaderTable.StrideInBytes * m_subtableMissEntries.entries.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    table.m_hitGroupTable.StrideInBytes = Entry::GetSize(m_subtableHitGroupEntries.maxNumParameters);
    table.m_hitGroupTable.SizeInBytes = Align(table.m_hitGroupTable.StrideInBytes * m_subtableHitGroupEntries.entries.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

    const auto bufferSize = table.m_rayGenerationShaderRecord.SizeInBytes + table.m_missShaderTable.SizeInBytes + table.m_hitGroupTable.SizeInBytes;
    table.m_table = GraphicsResource::CreateUploadHeap(L"shader binding table", bufferSize, device);

    // Copy data to table.
    {
        auto tableData = ScopedResourceMap(table.m_table);
        auto currentRecord = (uint8_t*)tableData.Get();

        m_rayGenProgramEntry.CopyData(raytracingPipelineProperties, currentRecord);
        currentRecord += table.m_rayGenerationShaderRecord.SizeInBytes;

        for (const auto& entry : m_subtableMissEntries.entries)
        {
            entry.CopyData(raytracingPipelineProperties, currentRecord);
            currentRecord += table.m_missShaderTable.StrideInBytes;
        }
        currentRecord = Align(currentRecord, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        assert(currentRecord == (uint8_t*)tableData.Get() + table.m_rayGenerationShaderRecord.SizeInBytes + table.m_missShaderTable.SizeInBytes);

        for (const auto& entry : m_subtableHitGroupEntries.entries)
        {
            entry.CopyData(raytracingPipelineProperties, currentRecord);
            currentRecord += table.m_hitGroupTable.StrideInBytes;
        }
        currentRecord = Align(currentRecord, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        assert(currentRecord == (uint8_t*)tableData.Get() + bufferSize);
    }

    // Fill out gpu addresses.
    table.m_rayGenerationShaderRecord.StartAddress = table.m_table->GetGPUVirtualAddress();
    table.m_missShaderTable.StartAddress = table.m_rayGenerationShaderRecord.StartAddress + table.m_rayGenerationShaderRecord.SizeInBytes;
    table.m_hitGroupTable.StartAddress = table.m_missShaderTable.StartAddress + table.m_missShaderTable.SizeInBytes;

    return table;
}

void RaytracingBindingTableGenerator::Group::AddEntry(const std::wstring & entryPoint, const std::vector<uint64_t>& inputData)
{
    entries.push_back(Entry{ entryPoint, inputData });
    maxNumParameters = std::max(maxNumParameters, inputData.size());
}

uint64_t RaytracingBindingTableGenerator::Entry::GetSize(size_t numParameters)
{
    return Align<uint64_t>(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8 * (uint64_t)numParameters, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
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
