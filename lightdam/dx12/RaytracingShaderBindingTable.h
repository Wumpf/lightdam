#pragma once

#include "GraphicsResource.h"
#include <string>
#include <vector>

class RaytracingShaderBindingTable
{
public:
    RaytracingShaderBindingTable() = default;
    void DispatchRays(ID3D12GraphicsCommandList4* commandList, uint32_t width, uint32_t height, uint32_t depth = 1);

private:
    friend class RaytracingBindingTableGenerator;

    GraphicsResource m_table;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE            m_rayGenerationShaderRecord;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE m_missShaderTable;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE m_hitGroupTable;
};

class RaytracingBindingTableGenerator
{
public:
    RaytracingShaderBindingTable Generate(ID3D12StateObjectProperties* raytracingPipelineProperties, ID3D12Device5* device);

    void SetRayGenProgram(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)   { m_rayGenProgramEntry = { entryPoint, inputData }; };
    void AddMissProgram(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)     { m_subtableMissEntries.AddEntry(entryPoint, inputData); }
    void AddHitGroupProgram(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData) { m_subtableHitGroupEntries.AddEntry(entryPoint, inputData); }

private:
    struct Entry
    {
        uint64_t GetSize() const { return GetSize(inputData.size()); }
        static uint64_t GetSize(size_t numParameters);

        void CopyData(ID3D12StateObjectProperties* raytracingPipelineProperties, uint8_t* destRecords) const;

        std::wstring programEntryPoint;
        std::vector<uint64_t> inputData;
    };


    struct Group
    {
        void AddEntry(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData);
        void CopyData(ID3D12StateObjectProperties* raytracingPipelineProperties, uint8_t*& destRecords) const;

        std::vector<Entry> entries;
        size_t maxNumParameters = 0;
    };

    Entry m_rayGenProgramEntry;
    Group m_subtableMissEntries;
    Group m_subtableHitGroupEntries;
};
