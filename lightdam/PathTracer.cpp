#include "PathTracer.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include <DirectXMath.h>

#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/GraphicsResource.h"
#include "ErrorHandling.h"
#include "MathUtils.h"
#include "Camera.h"

#include "../external/d3dx12.h"


struct GlobalConstants
{
    DirectX::XMVECTOR CameraU;
    DirectX::XMVECTOR CameraV;
    DirectX::XMVECTOR CameraW;
    DirectX::XMVECTOR CameraPosition;
};

PathTracer::PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight)
    : m_descriptorHeapIncrementSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
    , m_frameConstantBuffer(L"FrameConstants", device, sizeof(GlobalConstants))
{
    CreateRootSignatures(device);
    LoadShaders();
    CreateRaytracingPipelineObject(device);
    CreateDescriptorHeap(device);
    CreateOutputBuffer(device, outputWidth, outputHeight);
}

PathTracer::~PathTracer()
{
}

void PathTracer::SetScene(Scene& scene, ID3D12Device5* device)
{
    CreateShaderBindingTable(scene, device);

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_rayGenDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorHeapIncrementSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC tlasView = {};
    tlasView.Format = DXGI_FORMAT_UNKNOWN;
    tlasView.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    tlasView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    tlasView.RaytracingAccelerationStructure.Location = scene.GetTopLevelAccellerationStructure().GetGPUAddress();
    device->CreateShaderResourceView(nullptr, &tlasView, descriptorHandle);
}

void PathTracer::DrawIteration(ID3D12GraphicsCommandList4* commandList, const TextureResource& renderTarget, const Camera& activeCamera, int frameIndex)
{
    // Update per-frame constants.
    auto globalConstants = m_frameConstantBuffer.GetData<GlobalConstants>(frameIndex);
    activeCamera.ComputeCameraParams(globalConstants->CameraU, globalConstants->CameraV, globalConstants->CameraW);
    globalConstants->CameraPosition = activeCamera.GetPosition();

    // Setup global resources.
    ID3D12DescriptorHeap* heaps[] = { m_rayGenDescriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetComputeRootSignature(m_globalRootSignature.Get());
    commandList->SetComputeRootConstantBufferView(0, m_frameConstantBuffer.GetGPUAddress(frameIndex));

    // Transition output buffer from copy to unordered access - assumes we copied previously.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &transition);

    // Setup raytracing task
    commandList->SetPipelineState1(m_raytracingPipelineObject.Get());
    m_shaderBindingTable.DispatchRays(commandList, m_outputResource.GetWidth(), m_outputResource.GetHeight());

    // Copy raytracing output buffer to backbuffer.
    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(1, &transition);
    transition = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &transition);
    commandList->CopyResource(renderTarget.Get(), m_outputResource.Get());

    // Go back to render target state.
    transition = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &transition);
}

void PathTracer::CreateShaderBindingTable(Scene& scene, ID3D12Device5* device)
{
    RaytracingBindingTableGenerator bindingTableGenerator;

    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_rayGenDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = reinterpret_cast<uint64_t*>(srvUavHeapHandle.ptr);
    bindingTableGenerator.SetRayGenProgram(L"RayGen", { (uint64_t)heapPointer });
    bindingTableGenerator.AddMissProgram(L"Miss", {});
    for (const auto& mesh : scene.GetMeshes())
        bindingTableGenerator.AddHitGroupProgram(L"HitGroup", { mesh.vertexBuffer->GetGPUVirtualAddress() });

    m_shaderBindingTable = bindingTableGenerator.Generate(m_raytracingPipelineObjectProperties.Get(), device);
}

void PathTracer::CreateDescriptorHeap(ID3D12Device5* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NumDescriptors = 2;
    ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_rayGenDescriptorHeap)));
}

void PathTracer::CreateOutputBuffer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight)
{
    m_outputResource = TextureResource::CreateTexture2D(L"PathTracer outputBuffer", DXGI_FORMAT_R8G8B8A8_UNORM, outputWidth, outputHeight, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, device);

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_rayGenDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, descriptorHandle);
}

void PathTracer::LoadShaders()
{
    m_rayGenLibrary = Shader::CompileFromFile(L"RayGen.hlsl");
    m_missLibrary = Shader::CompileFromFile(L"Miss.hlsl");
    m_hitLibrary = Shader::CompileFromFile(L"Hit.hlsl");
}

static ComPtr<ID3D12RootSignature> CreateRootSignature(const wchar_t* name, ID3D12Device5* device, const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& desc)
{
    ComPtr<ID3DBlob> sigBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, nullptr));
    ComPtr<ID3D12RootSignature> signature;
    ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&signature)));
    signature->SetName(name);
    return signature;
}

void PathTracer::CreateRootSignatures(ID3D12Device5* device)
{
    // Global root signature
    {
        CD3DX12_ROOT_PARAMETER1 params[1] = {};
        params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(params), params);
        m_globalRootSignature = CreateRootSignature(L"PathTracerGlobalRootSig", device, rootSignatureDesc);
    }
    // RayGen local root signature
    {
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] =
        {
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC), // Output buffer in u0
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC), // TLAS in t0
        };
        CD3DX12_ROOT_PARAMETER1 param; param.InitAsDescriptorTable(_countof(descriptorRanges), descriptorRanges);
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_rayGenSignature = CreateRootSignature(L"RayGen", device, rootSignatureDesc);
    }
    // Miss local root signature
    {
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(0, (D3D12_ROOT_PARAMETER1*)nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_missSignature = CreateRootSignature(L"Miss", device, rootSignatureDesc);
    }
    // Hit local root signature.
    {
        CD3DX12_ROOT_PARAMETER1 param; param.InitAsShaderResourceView(0); // Vertex buffer in t0
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_hitSignature = CreateRootSignature(L"Hit", device, rootSignatureDesc);
    }
}

void PathTracer::CreateRaytracingPipelineObject(ID3D12Device5* device)
{
    CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Used libraries.
    {
        auto rayGenLib = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        rayGenLib->SetDXILLibrary(&m_rayGenLibrary.GetByteCode());
        rayGenLib->DefineExport(L"RayGen");

        auto missLib = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        missLib->SetDXILLibrary(&m_missLibrary.GetByteCode());
        missLib->DefineExport(L"Miss");

        auto closestHitLib = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        closestHitLib->SetDXILLibrary(&m_hitLibrary.GetByteCode());
        closestHitLib->DefineExport(L"ClosestHit");
    }
    // Shader config
    {
        auto config = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        config->Config(4 * sizeof(float),  // RGB + distance
                       2 * sizeof(float)); // barycentric coordinates
    }
    // Pipeline config.
    {
        auto config = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        config->Config(1);
    }
    // Hit groups to export association.
    {
        auto hitGroup = stateObjectDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetHitGroupExport(L"HitGroup");
        hitGroup->SetClosestHitShaderImport(L"ClosestHit");
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    // Local root signatures & association to hitgroup/type
    {
        {
            auto localRootSignature_rayGen = stateObjectDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature_rayGen->SetRootSignature(m_rayGenSignature.Get());
            auto localRootSignatureAssociation_rayGen = stateObjectDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            localRootSignatureAssociation_rayGen->SetSubobjectToAssociate(*localRootSignature_rayGen);
            localRootSignatureAssociation_rayGen->AddExport(L"RayGen");
        }
        {
            auto localRootSignature_miss = stateObjectDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature_miss->SetRootSignature(m_missSignature.Get());
            auto localRootSignatureAssociation_miss = stateObjectDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            localRootSignatureAssociation_miss->SetSubobjectToAssociate(*localRootSignature_miss);
            localRootSignatureAssociation_miss->AddExport(L"Miss");
        }
        {
            auto localRootSignature_hit = stateObjectDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature_hit->SetRootSignature(m_hitSignature.Get());
            auto localRootSignatureAssociation_hit= stateObjectDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            localRootSignatureAssociation_hit->SetSubobjectToAssociate(*localRootSignature_hit);
            localRootSignatureAssociation_hit->AddExport(L"HitGroup");
        }
    }

    // Global root signature
    {
        auto globalRootSignature = stateObjectDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(m_globalRootSignature.Get());
    }

    ThrowIfFailed(device->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&m_raytracingPipelineObject)));
    ThrowIfFailed(m_raytracingPipelineObject->QueryInterface(IID_PPV_ARGS(&m_raytracingPipelineObjectProperties)));
}
