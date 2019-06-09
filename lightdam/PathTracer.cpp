#include "PathTracer.h"
#include <dxcapi.h>


PathTracer::PathTracer()
{
    LoadShaders();
    CreateRaytracingPipelineObject();
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

void PathTracer::CreateRaytracingPipelineObject()
{
    //nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device.Get());

    //pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
    //pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
    //pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit" });

    //m_rayGenSignature = CreateRayGenSignature();
    //m_missSignature = CreateMissSignature();
    //m_hitSignature = CreateHitSignature();

    //pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");

    //pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
    //pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" });
    //pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup" });

    //pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance
    //pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates
    //pipeline.SetMaxRecursionDepth(1);

    //m_rtStateObject = pipeline.Generate();
    //ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
}