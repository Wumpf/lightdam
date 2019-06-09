#include "PathTracer.h"
#include <vector>
#include <string>
#include <stdexcept>
#include "ErrorHandling.h"
#include "RaytracingPipelineGenerator.h"
#include "../external/d3dx12.h"

PathTracer::PathTracer(ID3D12Device5* device)
{
    CreateLocalRootSignatures(device);
    LoadShaders();
    CreateRaytracingPipelineObject(device);
}

PathTracer::~PathTracer()
{
}

void PathTracer::SetScene(Scene& scene)
{
    CreateShaderBindingTable();
}

void PathTracer::CreateShaderBindingTable()
{
    //m_sbtHelper.Reset();

    //D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    //auto heapPointer = reinterpret_cast<uint64_t*>(srvUavHeapHandle.ptr);
    //m_sbtHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });
    //m_sbtHelper.AddMissProgram(L"Miss", {});
    //m_sbtHelper.AddHitGroup(L"HitGroup", { (void*)(m_vertexBuffer->GetGPUVirtualAddress()) });

    //const uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();
    //m_sbtStorage = nv_helpers_dx12::CreateBuffer(m_device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
    //if (!m_sbtStorage)
    //{
    //    throw std::logic_error("Could not allocate the shader binding table");
    //}

    //m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
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
}