#pragma once

#include "dx12/Shader.h"
#include "dx12/GraphicsResource.h"
#include "dx12/DynamicConstantBuffer.h"
#include "dx12/RaytracingShaderBindingTable.h"
#include "LightSampler.h"
#include "Camera.h"
#include <random>

struct IDxcBlob;
class Scene;

class PathTracer
{
public:

    PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight);
    ~PathTracer();

    void ResizeOutput(uint32_t outputWidth, uint32_t outputHeight);
    void ReloadShaders();
    void RestartSampling();
    void SetScene(Scene& scene);

    bool GetPathLengthFilterEnabled() const                 { return m_enablePathLengthFilter; }
    void SetPathLengthFilterEnabled(bool enablePathLengthFilter, class Application& application);
    float GetPathLengthFilterMax() const                    { return m_pathLengthFilterMax; }
    void SetPathLengthFilterMax(float pathLengthFilterMax)  { m_pathLengthFilterMax = pathLengthFilterMax; RestartSampling(); }


    void DrawIteration(ID3D12GraphicsCommandList4* commandList, const Camera& activeCamera);

    // Returns descriptor handle for output texture, living in the descriptor heap used and set by the pathtracer.
    const D3D12_GPU_DESCRIPTOR_HANDLE& GetOutputTextureDescHandle() const { return m_outputGPUDescriptorHandleSRV; }
    void SetDescriptorHeap(ID3D12GraphicsCommandList4* commandList);

    // Returns output texture reosurce
    const TextureResource& GetOutputTextureResource() const { return m_outputResource; }

    // Number of frames sent to the GPU == number of samples per Pixel.
    uint32_t GetScheduledIterationNumber() const { return m_frameNumber; }

private:
    bool LoadShaders(bool throwOnFailure);
    void CreateRootSignatures(uint32_t maxNumMeshes, uint32_t maxNumTextures);
    void CreateRaytracingPipelineObject();
    void CreateShaderBindingTable(const Scene& scene);
    void CreateDescriptorHeap(const Scene& scene);

    void CreateOutputBuffer(uint32_t outputWidth, uint32_t outputHeight);
    void WriteOutputBufferDescriptorsToDescriptorHeap();

    ComPtr<ID3D12Device5> m_device;

    ComPtr<ID3D12DescriptorHeap> m_staticDescriptorHeap;

    TextureResource m_outputResource;
    //D3D12_GPU_DESCRIPTOR_HANDLE m_outputGPUDescriptorHandleUAV;
    D3D12_GPU_DESCRIPTOR_HANDLE m_outputGPUDescriptorHandleSRV;

    DynamicConstantBuffer m_frameConstantBuffer;
    Camera m_lastCamera;

    DynamicConstantBuffer m_areaLightSamples;
    static const uint32_t m_numAreaLightSamples = 32;
    std::unique_ptr<class LightSampler> m_lightSampler;


    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;
    Shader m_shadowRayLibrary;

    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12RootSignature> m_signatureEmpty;
    ComPtr<ID3D12RootSignature> m_signatureSceneData;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineObjectProperties;

    RaytracingBindingTableGenerator m_bindingTableGenerator;
    RaytracingShaderBindingTable m_shaderBindingTable;

    uint32_t m_frameNumber;
    std::mt19937 m_randomGenerator;

    const uint32_t m_descriptorHeapIncrementSize;

    bool m_enablePathLengthFilter = false;
    float m_pathLengthFilterMax = 10.0f;
};
