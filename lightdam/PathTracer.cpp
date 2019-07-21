#include "PathTracer.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>

#include <DirectXMath.h>

#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/GraphicsResource.h"
#include "dx12/RootSignature.h"
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

    DirectX::XMFLOAT2 GlobalJitter;
    uint32_t FrameNumber;
    int32_t _padding;
};

static const int randomSeed = 123;

PathTracer::PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight)
    : m_device(device)
    , m_descriptorHeapIncrementSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
    , m_frameConstantBuffer(L"FrameConstants", device, sizeof(GlobalConstants))
    , m_frameNumber(0)
    , m_randomGenerator(randomSeed)
{
    LoadShaders(true);
    CreateRootSignatures();
    CreateRaytracingPipelineObject();
    CreateDescriptorHeap();
    CreateOutputBuffer(outputWidth, outputHeight);
}

PathTracer::~PathTracer()
{
}

void PathTracer::ResizeOutput(uint32_t outputWidth, uint32_t outputHeight)
{
    CreateOutputBuffer(outputWidth, outputHeight);
    RestartSampling();
}

void PathTracer::ReloadShaders()
{
    if (LoadShaders(false))
    {
        CreateRaytracingPipelineObject();
        m_shaderBindingTable = m_bindingTableGenerator.Generate(m_raytracingPipelineObjectProperties.Get(), m_device.Get());
        RestartSampling();
    }
}

void PathTracer::SetScene(Scene& scene)
{
    CreateShaderBindingTable(scene);

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_staticDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC tlasView = {};
        tlasView.Format = DXGI_FORMAT_UNKNOWN;
        tlasView.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        tlasView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        tlasView.RaytracingAccelerationStructure.Location = scene.GetTopLevelAccellerationStructure().GetGPUAddress();
        m_device->CreateShaderResourceView(nullptr, &tlasView, descriptorHandle);
    }
}

void PathTracer::DrawIteration(ID3D12GraphicsCommandList4* commandList, const Camera& activeCamera)
{
    if (m_lastCamera != activeCamera)
    {
        RestartSampling();
        m_lastCamera = activeCamera;
    }

    std::uniform_real_distribution<float> randomNumberDistribution(0.0f, 1.0f);

    // Update per-frame constants.
    uint32_t frameIndex = m_frameNumber % m_frameConstantBuffer.GetNumBuffers();
    auto globalConstants = m_frameConstantBuffer.GetData<GlobalConstants>(frameIndex);
    activeCamera.ComputeCameraParams((float)m_outputResource.GetWidth() / m_outputResource.GetHeight(), globalConstants->CameraU, globalConstants->CameraV, globalConstants->CameraW);
    globalConstants->CameraPosition = activeCamera.GetPosition();
    globalConstants->GlobalJitter.x = randomNumberDistribution(m_randomGenerator);  // hammersley/halton would likely be better for this usecase, but we take so many samples that it doesn't really metter
    globalConstants->GlobalJitter.y = randomNumberDistribution(m_randomGenerator);
    globalConstants->FrameNumber = m_frameNumber;

    // Transition output buffer from copy to unordered access - assume it starts as pixel shader resource.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &transition);

    // Setup global resources.
    ID3D12DescriptorHeap* heaps[] = { m_staticDescriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetComputeRootSignature(m_globalRootSignature.Get());
    commandList->SetComputeRootConstantBufferView(0, m_frameConstantBuffer.GetGPUAddress(frameIndex));
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_staticDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_descriptorHeapIncrementSize);
    commandList->SetComputeRootDescriptorTable(1, cbvHandle);

    // Setup raytracing task
    commandList->SetPipelineState1(m_raytracingPipelineObject.Get());
    m_shaderBindingTable.DispatchRays(commandList, m_outputResource.GetWidth(), m_outputResource.GetHeight());

    // Set output back to pixel shader resource.
    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &transition);

    ++m_frameNumber;
}

void PathTracer::CreateShaderBindingTable(Scene& scene)
{
    m_bindingTableGenerator.Clear();
    
    m_bindingTableGenerator.SetRayGenProgram(L"RayGen", {});
    m_bindingTableGenerator.AddMissProgram(L"Miss", {});
    m_bindingTableGenerator.AddMissProgram(L"ShadowMiss", {});
    for (const auto& mesh : scene.GetMeshes())
    {
        m_bindingTableGenerator.AddHitGroupProgram(L"HitGroup", { mesh.vertexBuffer->GetGPUVirtualAddress(), mesh.indexBuffer->GetGPUVirtualAddress() });
        m_bindingTableGenerator.AddHitGroupProgram(L"ShadowHitGroup", { });
    }
    m_shaderBindingTable = m_bindingTableGenerator.Generate(m_raytracingPipelineObjectProperties.Get(), m_device.Get());
}

void PathTracer::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NumDescriptors = 3;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_staticDescriptorHeap)));
}

void PathTracer::CreateOutputBuffer(uint32_t outputWidth, uint32_t outputHeight)
{
    const auto format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    m_outputResource = TextureResource::CreateTexture2D(L"PathTracer outputBuffer", format, outputWidth, outputHeight, 1,
                                                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_device.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandleCPU(m_staticDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandleGPU(m_staticDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // Descriptor for UAV.
    {
        descriptorHandleCPU.Offset(m_descriptorHeapIncrementSize);
        descriptorHandleGPU.Offset(m_descriptorHeapIncrementSize); //m_outputGPUDescriptorHandleUAV = descriptorHandleGPU.Offset(m_descriptorHeapIncrementSize);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        m_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, descriptorHandleCPU);
    }

    // Descriptor for SRV.
    {
        descriptorHandleCPU.Offset(m_descriptorHeapIncrementSize);
        m_outputGPUDescriptorHandleSRV = descriptorHandleGPU.Offset(m_descriptorHeapIncrementSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_outputResource.Get(), &srvDesc, descriptorHandleCPU);
    }
}

void PathTracer::RestartSampling()
{
    m_frameNumber = 0;
    m_randomGenerator.seed(randomSeed);
}

bool PathTracer::LoadShaders(bool throwOnFailure)
{
    const Shader::LoadInstruction shaderLoads[] =
    {
        { Shader::Type::Library, L"shaders/RayGen.hlsl", L"" },
        { Shader::Type::Library, L"shaders/Miss.hlsl", L"" },
        { Shader::Type::Library, L"shaders/Hit.hlsl", L"" },
        { Shader::Type::Library, L"shaders/ShadowRay.hlsl", L"" },
    };
    Shader* shaders[] =
    {
        &m_rayGenLibrary,
        &m_missLibrary,
        &m_hitLibrary,
        &m_shadowRayLibrary,
    };
    return Shader::ReplaceShadersOnSuccessfulCompileFromFiles(shaderLoads, shaders, throwOnFailure);
}

void PathTracer::CreateRootSignatures()
{
    // Global root signature
    {
        CD3DX12_ROOT_PARAMETER1 params[2] = {};
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] =
        {
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC),                      // TLAS in t0, space0
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE), // Output buffer in u0,space0
        };
        params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);  // Global constant buffer at c0
        params[1].InitAsDescriptorTable(_countof(descriptorRanges), descriptorRanges);
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(params), params);
        m_globalRootSignature = CreateRootSignature(L"PathTracerGlobalRootSig", m_device.Get(), rootSignatureDesc);
    }
    // Empty local root signature
    {
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(0, (D3D12_ROOT_PARAMETER1*)nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_signatureEmpty = CreateRootSignature(L"EmptyLocalRootSignature", m_device.Get(), rootSignatureDesc);
    }
    // Hit local root signature.
    {
        CD3DX12_ROOT_PARAMETER1 params[2];  // TODO: would be bette as descriptor heap since it would reduce number of things set per mesh!
        params[0].InitAsShaderResourceView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC); // Vertex buffer in t0,space1
        params[1].InitAsShaderResourceView(0, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC); // Index buffer in t0,space2
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_signatureSceneData = CreateRootSignature(L"SceneData", m_device.Get(), rootSignatureDesc);
    }
}

void PathTracer::CreateRaytracingPipelineObject()
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

        auto shadowRayLib = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        shadowRayLib->SetDXILLibrary(&m_shadowRayLibrary.GetByteCode());
        shadowRayLib->DefineExport(L"ShadowMiss");
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
        config->Config(2); // MaxRecursionDepth - ray + shadow ray
    }
    // Hit groups to export association.
    {
        auto hitGroup = stateObjectDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetHitGroupExport(L"HitGroup");
        hitGroup->SetClosestHitShaderImport(L"ClosestHit");
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }
    {
        auto shadowHitGroup = stateObjectDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        shadowHitGroup->SetHitGroupExport(L"ShadowHitGroup");
        shadowHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    // Local root signatures & association to hitgroup/type
    {
        {
            auto localRootSignature_miss = stateObjectDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature_miss->SetRootSignature(m_signatureEmpty.Get());
            auto localRootSignatureAssociation_noSceneData = stateObjectDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            localRootSignatureAssociation_noSceneData->SetSubobjectToAssociate(*localRootSignature_miss);
            localRootSignatureAssociation_noSceneData->AddExport(L"Miss");
            localRootSignatureAssociation_noSceneData->AddExport(L"ShadowHitGroup");
            localRootSignatureAssociation_noSceneData->AddExport(L"RayGen");
        }
        {
            auto localRootSignature_hit = stateObjectDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature_hit->SetRootSignature(m_signatureSceneData.Get());
            auto localRootSignatureAssociation_hit = stateObjectDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            localRootSignatureAssociation_hit->SetSubobjectToAssociate(*localRootSignature_hit);
            localRootSignatureAssociation_hit->AddExport(L"HitGroup");
        }
    }

    // Global root signature
    {
        auto globalRootSignature = stateObjectDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(m_globalRootSignature.Get());
    }

    ThrowIfFailed(m_device->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&m_raytracingPipelineObject)));
    ThrowIfFailed(m_raytracingPipelineObject->QueryInterface(IID_PPV_ARGS(&m_raytracingPipelineObjectProperties)));
}
