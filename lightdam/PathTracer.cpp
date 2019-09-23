#include "PathTracer.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include <DirectXMath.h>

#include "Application.h"
#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/GraphicsResource.h"
#include "dx12/RootSignature.h"
#include "ErrorHandling.h"
#include "MathUtils.h"
#include "Camera.h"
#include "LightSampler.h"

#include "../external/d3dx12.h"

struct GlobalConstants
{
    DirectX::XMVECTOR CameraU;
    DirectX::XMVECTOR CameraV;
    DirectX::XMVECTOR CameraW;
    DirectX::XMVECTOR CameraPosition;

    DirectX::XMFLOAT2 GlobalJitter;
    uint32_t FrameNumber;
    uint32_t FrameSeed;

    float PathLengthFilterMax;
    float _padding[3];
};

static const int randomSeed = 123;

enum class HitShader
{
    Regular,
    Emitter,
    Metal,

    Count
};
static const wchar_t* s_hitShaderNames[(int)HitShader::Count] = { L"ClosestHit_Regular", L"ClosestHit_Emitter", L"ClosestHit_Metal" };
static const wchar_t* s_hitGroupNames[(int)HitShader::Count] = { L"HitGroup_Regular", L"HitGroup_Emitter", L"HitGroup_Metal" };

PathTracer::PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight)
    : m_device(device)
    , m_descriptorHeapIncrementSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
    , m_frameConstantBuffer(L"FrameConstants", device, sizeof(GlobalConstants))
    , m_areaLightSamples(L"AreaLightSamples", device, sizeof(LightSampler::LightSample) * m_numAreaLightSamples)
    , m_frameNumber(0)
    , m_randomGenerator(randomSeed)
{
    LoadShaders(true);
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
    CreateDescriptorHeap(scene);
    CreateRootSignatures((uint32_t)scene.GetMeshes().size(), (uint32_t)scene.GetTextures().size());
    CreateRaytracingPipelineObject();
    CreateShaderBindingTable(scene);
    m_lightSampler.reset(new LightSampler(scene.GetAreaLights()));
}

void PathTracer::SetPathLengthFilterEnabled(bool enablePathLengthFilter, Application& application)
{
    if (m_enablePathLengthFilter == enablePathLengthFilter)
        return;

    m_enablePathLengthFilter = enablePathLengthFilter;
    application.WaitForGPUOnNextFrameFinishAndExecute([this]() { ReloadShaders(); });
}

void PathTracer::SetDescriptorHeap(ID3D12GraphicsCommandList4* commandList)
{
    ID3D12DescriptorHeap* heaps[] = { m_staticDescriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
}

void PathTracer::DrawIteration(ID3D12GraphicsCommandList4* commandList, const Camera& activeCamera)
{
    if (m_lastCamera != activeCamera)
    {
        RestartSampling();
        m_lastCamera = activeCamera;
    }

    // Update per-frame constants.
    uint32_t frameIndex = m_frameNumber % m_frameConstantBuffer.GetNumBuffers();
    auto globalConstants = m_frameConstantBuffer.GetData<GlobalConstants>(frameIndex);
    activeCamera.ComputeCameraParams((float)m_outputResource.GetWidth() / m_outputResource.GetHeight(), globalConstants->CameraU, globalConstants->CameraV, globalConstants->CameraW);
    globalConstants->CameraPosition = activeCamera.GetPosition();
    globalConstants->GlobalJitter.x = ComputeHaltonSequence(m_frameNumber, 0);
    globalConstants->GlobalJitter.y = ComputeHaltonSequence(m_frameNumber, 1);
    globalConstants->FrameNumber = m_frameNumber;
    globalConstants->FrameSeed = m_randomGenerator();
    globalConstants->PathLengthFilterMax = m_pathLengthFilterMax;
    m_lightSampler->GenerateRandomSamples(m_frameNumber, m_areaLightSamples.GetData<LightSampler::LightSample>(frameIndex), m_numAreaLightSamples);

    // Transition output buffer from copy to unordered access - assume it starts as pixel shader resource.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &transition);

    // Setup global resources.
    SetDescriptorHeap(commandList);
    commandList->SetComputeRootSignature(m_globalRootSignature.Get());
    commandList->SetComputeRootConstantBufferView(0, m_frameConstantBuffer.GetGPUAddress(frameIndex));
    commandList->SetComputeRootConstantBufferView(1, m_areaLightSamples.GetGPUAddress(frameIndex));
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_staticDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_descriptorHeapIncrementSize); // skip first entry == output srv
    commandList->SetComputeRootDescriptorTable(2, cbvHandle);

    // Setup raytracing task
    commandList->SetPipelineState1(m_raytracingPipelineObject.Get());
    m_shaderBindingTable.DispatchRays(commandList, m_outputResource.GetWidth(), m_outputResource.GetHeight());

    // Set output back to pixel shader resource.
    transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &transition);

    ++m_frameNumber;
}

void PathTracer::CreateShaderBindingTable(const Scene& scene)
{
    m_bindingTableGenerator.Clear();
    
    m_bindingTableGenerator.SetRayGenProgram(L"RayGen", {});
    m_bindingTableGenerator.AddMissProgram(L"Miss", {});
    m_bindingTableGenerator.AddMissProgram(L"ShadowMiss", {});
    for (const auto& mesh : scene.GetMeshes())
    {
        if (mesh.isEmitter)
            m_bindingTableGenerator.AddHitGroupProgram(s_hitGroupNames[(int)HitShader::Emitter], { mesh.constantBuffer->GetGPUVirtualAddress() });
        else if (mesh.isMetal)
            m_bindingTableGenerator.AddHitGroupProgram(s_hitGroupNames[(int)HitShader::Metal], { mesh.constantBuffer->GetGPUVirtualAddress() });
        else
            m_bindingTableGenerator.AddHitGroupProgram(s_hitGroupNames[(int)HitShader::Regular], { mesh.constantBuffer->GetGPUVirtualAddress() });
        m_bindingTableGenerator.AddHitGroupProgram(L"ShadowHitGroup", { });
    }
    m_shaderBindingTable = m_bindingTableGenerator.Generate(m_raytracingPipelineObjectProperties.Get(), m_device.Get());
}

void PathTracer::CreateDescriptorHeap(const Scene& scene)
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // output srv, output uav, scene tlas, vertex buffers, index buffers, textures
    descriptorHeapDesc.NumDescriptors = (UINT)(3 + scene.GetMeshes().size() * 2 + scene.GetTextures().size());
    ThrowIfFailed(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_staticDescriptorHeap)));

    WriteOutputBufferDescriptorsToDescriptorHeap();

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_staticDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorHeapIncrementSize * 2);

    // TLAS
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC tlasView = {};
        tlasView.Format = DXGI_FORMAT_UNKNOWN;
        tlasView.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        tlasView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        tlasView.RaytracingAccelerationStructure.Location = scene.GetTopLevelAccellerationStructure().GetGPUAddress();
        m_device->CreateShaderResourceView(nullptr, &tlasView, descriptorHandle);
    }

    // Vertex Buffers
    for (const auto& mesh : scene.GetMeshes())
    {
        descriptorHandle.Offset(m_descriptorHeapIncrementSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC vertexBufferView = {};
        vertexBufferView.Format = DXGI_FORMAT_UNKNOWN;
        vertexBufferView.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexBufferView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vertexBufferView.Buffer.FirstElement = 0;
        vertexBufferView.Buffer.NumElements = mesh.vertexCount;
        vertexBufferView.Buffer.StructureByteStride = sizeof(Scene::Vertex);
        vertexBufferView.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_device->CreateShaderResourceView(mesh.vertexBuffer.Get(), &vertexBufferView, descriptorHandle);
    }

    // Index Buffers
    for (const auto& mesh : scene.GetMeshes())
    {
        descriptorHandle.Offset(m_descriptorHeapIncrementSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC indexBufferView = {};
        indexBufferView.Format = DXGI_FORMAT_UNKNOWN;
        indexBufferView.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        indexBufferView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        indexBufferView.Buffer.FirstElement = 0;
        indexBufferView.Buffer.NumElements = mesh.indexCount;
        indexBufferView.Buffer.StructureByteStride = sizeof(uint32_t);
        indexBufferView.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_device->CreateShaderResourceView(mesh.indexBuffer.Get(), &indexBufferView, descriptorHandle);
    }

    // Textures
    for (const auto& texture : scene.GetTextures())
    {
        descriptorHandle.Offset(m_descriptorHeapIncrementSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC textureView = {};
        textureView.Format = DXGI_FORMAT_UNKNOWN;
        textureView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        textureView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        textureView.Texture2D.MostDetailedMip = 0;
        textureView.Texture2D.MipLevels = -1;
        textureView.Texture2D.PlaneSlice = 0;
        textureView.Texture2D.ResourceMinLODClamp = 0.0f;

        m_device->CreateShaderResourceView(texture.Get(), &textureView, descriptorHandle);
    }
}

void PathTracer::CreateOutputBuffer(uint32_t outputWidth, uint32_t outputHeight)
{
    m_outputResource = TextureResource::CreateTexture2D(L"PathTracer outputBuffer", DXGI_FORMAT_R32G32B32A32_FLOAT, outputWidth, outputHeight, 1,
                                                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_device.Get());
    WriteOutputBufferDescriptorsToDescriptorHeap();
}

void PathTracer::WriteOutputBufferDescriptorsToDescriptorHeap()
{
    if (!m_staticDescriptorHeap)
        return;

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandleCPU(m_staticDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Descriptor for SRV.
    {
        m_outputGPUDescriptorHandleSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_staticDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_outputResource.GetFormat();
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_outputResource.Get(), &srvDesc, descriptorHandleCPU);
    }

    descriptorHandleCPU.Offset(m_descriptorHeapIncrementSize);

    // Descriptor for UAV.
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        m_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, descriptorHandleCPU);
    }
}

void PathTracer::RestartSampling()
{
    m_frameNumber = 0;
    m_randomGenerator.seed(randomSeed);
}

bool PathTracer::LoadShaders(bool throwOnFailure)
{
    std::vector<DxcDefine> preprocessorDefines;
    if (m_enablePathLengthFilter)
        preprocessorDefines.push_back(DxcDefine{ L"ENABLE_PATHLENGTH_FILTER", nullptr });

    const Shader::LoadInstruction shaderLoads[] =
    {
        { Shader::Type::Library, L"shaders/RayGen.hlsl", L"", preprocessorDefines },
        { Shader::Type::Library, L"shaders/Miss.hlsl", L"", preprocessorDefines },
        { Shader::Type::Library, L"shaders/Hit.hlsl", L"", preprocessorDefines },
        { Shader::Type::Library, L"shaders/ShadowRay.hlsl", L"", preprocessorDefines },
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

void PathTracer::CreateRootSignatures(uint32_t maxNumMeshes, uint32_t maxNumTextures)
{
    // Global root signature
    {
        CD3DX12_ROOT_PARAMETER1 params[3] = {};
        CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] =
        {
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),                    // Output buffer in u0,space0
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC),                      // TLAS in t0, space0
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, maxNumMeshes, 0, 100, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC),         // Vertex buffers
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, maxNumMeshes, 0, 101, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC),         // Index buffers
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, maxNumTextures, 0, 102, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC),       // Textures
        };
        params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);  // Global constant buffer at b0
        params[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);  // Area light sample constant buffer at b1
        params[2].InitAsDescriptorTable(_countof(descriptorRanges), descriptorRanges);
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
        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1];
        staticSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

        CD3DX12_ROOT_PARAMETER1 params[1];
        params[0].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC); // Constant buffer at b2
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(params), params, _countof(staticSamplers), staticSamplers, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
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
        closestHitLib->DefineExports(s_hitShaderNames);

        auto shadowRayLib = stateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        shadowRayLib->SetDXILLibrary(&m_shadowRayLibrary.GetByteCode());
        shadowRayLib->DefineExport(L"ShadowMiss");
    }
    // Shader config
    {
        auto config = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        config->Config(8 * sizeof(float),
                       2 * sizeof(float)); // barycentric coordinates
    }
    // Pipeline config.
    {
        auto config = stateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        config->Config(2); // MaxRecursionDepth - ray + shadow ray
    }
    // Hit groups to export association.
    for (int i=0; i< (int)HitShader::Count; ++i)
    {
        auto hitGroup = stateObjectDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetHitGroupExport(s_hitGroupNames[i]);
        hitGroup->SetClosestHitShaderImport(s_hitShaderNames[i]);
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
            localRootSignatureAssociation_hit->AddExports(s_hitGroupNames);
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
