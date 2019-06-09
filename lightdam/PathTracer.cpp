#include "PathTracer.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include "Scene.h"
#include "GraphicsResource.h"
#include "ErrorHandling.h"
#include "RaytracingPipelineGenerator.h"
#include "MathUtils.h"

#include "../external/d3dx12.h"

PathTracer::PathTracer(ID3D12Device5* device)
{
    CreateLocalRootSignatures(device);
    LoadShaders();
    CreateRaytracingPipelineObject(device);
    CreateSceneIndependentShaderResources(device);
}

PathTracer::~PathTracer()
{
}

void PathTracer::SetScene(Scene& scene, ID3D12Device5* device)
{
    CreateShaderBindingTable(scene, device);
}

class BindingTableGenerator
{
public:
    GraphicsResource Generate(ID3D12StateObjectProperties* raytracingPipelineProperties, ID3D12Device5* device)
    {
        // All entries are made up of an shader identifier and parameters each with 8 bytes.
        // And all need to have the same fixed size!
        const size_t recordSize = Align<size_t>(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8 * m_maxNumParameters, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        const size_t bufferSize = recordSize * (m_rayGenEntries.size() + m_missEntries.size() + m_hitGroupEntries.size());

        GraphicsResource table = GraphicsResource::CreateUploadHeap(L"shader binding table", bufferSize, device);
        auto tableData = ScopedResourceMap(table);
        auto currentRecord = (uint8_t*)tableData.Get();
        CopyData(m_rayGenEntries, raytracingPipelineProperties, recordSize, currentRecord);
        CopyData(m_missEntries, raytracingPipelineProperties, recordSize, currentRecord);
        CopyData(m_hitGroupEntries, raytracingPipelineProperties, recordSize, currentRecord);
        assert(currentRecord == (uint8_t*)tableData.Get() + bufferSize);

        return table;
    }

    void AddRayGenProgram(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)   { AddEntry(m_rayGenEntries, entryPoint, inputData); }
    void AddMissProgram(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)     { AddEntry(m_missEntries, entryPoint, inputData); }
    void AddHitGroup(const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)        { AddEntry(m_hitGroupEntries, entryPoint, inputData); }

private:
    struct Entry
    {
        const std::wstring programEntryPoint;
        const std::vector<uint64_t> inputData;
    };

    static void CopyData(const std::vector<Entry>& entries, ID3D12StateObjectProperties* raytracingPipelineProperties, size_t recordSize, uint8_t*& destRecords)
    {
        for (const auto& entry : entries)
        {
            void* id = raytracingPipelineProperties->GetShaderIdentifier(entry.programEntryPoint.c_str());
            if (!id)
            {
                std::wstring errMsg(std::wstring(L"Unknown shader identifier used in the SBT: ") + entry.programEntryPoint);
                throw errMsg;// std::logic_error(std::string(errMsg.begin(), errMsg.end())); // TODO
            }
            memcpy(destRecords, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            memcpy(destRecords + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, entry.inputData.data(), entry.inputData.size() * 8);
            destRecords += recordSize;
        }
    }

    void AddEntry(std::vector<Entry>& entryList, const std::wstring& entryPoint, const std::vector<uint64_t>& inputData)
    {
        entryList.push_back(Entry{ entryPoint, inputData });
        m_maxNumParameters = std::max(m_maxNumParameters, inputData.size());
    }

    size_t m_maxNumParameters = 0;
    std::vector<Entry> m_rayGenEntries;
    std::vector<Entry> m_missEntries;
    std::vector<Entry> m_hitGroupEntries;
};

void PathTracer::CreateShaderBindingTable(Scene& scene, ID3D12Device5* device)
{
    BindingTableGenerator bindingTableGenerator;

    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = reinterpret_cast<uint64_t*>(srvUavHeapHandle.ptr);
    bindingTableGenerator.AddRayGenProgram(L"RayGen", { (uint64_t)heapPointer });
    bindingTableGenerator.AddMissProgram(L"Miss", {});
    for (const auto& mesh : scene.GetMeshes())
        bindingTableGenerator.AddHitGroup(L"HitGroup", { mesh.vertexBuffer->GetGPUVirtualAddress() });

    m_shaderBindingTable = bindingTableGenerator.Generate(m_raytracingPipelineObjectProperties.Get(), device);
}

void PathTracer::CreateSceneIndependentShaderResources(ID3D12Device5* device)
{
    // TODO
//    ComPtr<ID3D12Resource> m_outputResource[SwapChain::MaxFramesInFlight];
//    ComPtr<ID3D12DescriptorHeap> m_rayGenDescriptorHeap;
}

void PathTracer::LoadShaders()
{
    m_rayGenLibrary = Shader::CompileFromFile(L"RayGen.hlsl");
    m_missLibrary = Shader::CompileFromFile(L"Miss.hlsl");
    m_hitLibrary = Shader::CompileFromFile(L"Hit.hlsl");
}

static ComPtr<ID3D12RootSignature> CreateRootSignature(const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& desc, const wchar_t* name, ID3D12Device5* device)
{
    ComPtr<ID3DBlob> sigBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, nullptr));
    ComPtr<ID3D12RootSignature> signature;
    ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&signature)));
    signature->SetName(name);
    return signature;
}

void PathTracer::CreateLocalRootSignatures(ID3D12Device5* device)
{
    // RayGen local root signature
    {
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] =
        {
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC), // Output buffer in u0
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC), // TLAS in t0
        };
        CD3DX12_ROOT_PARAMETER1 param; param.InitAsDescriptorTable(_countof(descriptorRanges), descriptorRanges);
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_rayGenSignature = CreateRootSignature(rootSignatureDesc, L"RayGen", device);
    }
    // Miss local root signature
    {
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(0, (D3D12_ROOT_PARAMETER1*)nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_missSignature = CreateRootSignature(rootSignatureDesc, L"Miss", device);
    }
    // Hit local root signature.
    {
        CD3DX12_ROOT_PARAMETER1 param; param.InitAsShaderResourceView(0); // Vertex buffer in t0
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_hitSignature = CreateRootSignature(rootSignatureDesc, L"Hit", device);
    }
}

void PathTracer::CreateRaytracingPipelineObject(ID3D12Device5* device)
{
    // TODO: Use CD3DX12_STATE_OBJECT_DESC instead and remove Nvidia's RayTracingPipelineGenerator
    RayTracingPipelineGenerator pipeline(device);

    pipeline.AddLibrary(m_rayGenLibrary.GetBlob(), { L"RayGen" });
    pipeline.AddLibrary(m_missLibrary.GetBlob(), { L"Miss" });
    pipeline.AddLibrary(m_hitLibrary.GetBlob(), { L"ClosestHit" });

    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");

    pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
    pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" });
    pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup" });

    pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates
    pipeline.SetMaxRecursionDepth(1);

    m_raytracingPipelineObject = pipeline.Generate();
    ThrowIfFailed(m_raytracingPipelineObject->QueryInterface(IID_PPV_ARGS(&m_raytracingPipelineObjectProperties)));
}
