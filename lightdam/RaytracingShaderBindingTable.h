#pragma once

#include "GraphicsResource.h"
#include <string>
#include <vector>

class RaytracingShaderBindingTable : public GraphicsResource
{
public:
    struct Subtable
    {
        uint32_t totalSize;
        uint32_t stride;
    };

    RaytracingShaderBindingTable() = default;
    RaytracingShaderBindingTable(GraphicsResource&& resource, uint32_t rayGenProgramEntrySize, Subtable subtableMiss, Subtable subtableHitGroups);


    void DispatchRays(ID3D12GraphicsCommandList4* commandList, uint32_t width, uint32_t height, uint32_t depth = 1);

private:
    uint32_t m_rayGenProgramEntrySize;
    Subtable m_subtableMiss;
    Subtable m_subtableHitGroups;
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
        uint32_t GetSize() const { return GetSize(inputData.size()); }
        static uint32_t GetSize(size_t numParameters);

        void CopyData(ID3D12StateObjectProperties* raytracingPipelineProperties, uint8_t* destRecords) const;

        std::wstring programEntryPoint;
        std::vector<uint64_t> inputData;
    };

    struct Group
    {
        void AddEntry(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData);
        RaytracingShaderBindingTable::Subtable ComputeTableSize();
        void CopyData(ID3D12StateObjectProperties* raytracingPipelineProperties, uint8_t*& destRecords) const;

        std::vector<Entry> entries;
        size_t maxNumParameters = 0;
    };

    Entry m_rayGenProgramEntry;
    Group m_subtableMissEntries;
    Group m_subtableHitGroupEntries;
};
