#pragma once

#include "dx12/SwapChain.h"
#include "dx12/Shader.h"
#include "dx12/GraphicsResource.h"
#include "dx12/DynamicConstantBuffer.h"
#include "dx12/RaytracingShaderBindingTable.h"
#include <random>

struct IDxcBlob;
class Scene;

class PathTracer
{
public:
    PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight);
    ~PathTracer();

    void ResizeOutput(uint32_t outputWidth, uint32_t outputHeight) { CreateOutputBuffer(outputWidth, outputHeight); }
    void ReloadShaders();
    void SetScene(Scene& scene);

    void DrawIteration(ID3D12GraphicsCommandList4* commandList, const class Camera& activeCamera);

    // Returns descriptor handle for output texture, living in the descriptor heap used and set by the pathtracer.
    const D3D12_GPU_DESCRIPTOR_HANDLE& GetOutputTextureDescHandle() const     { return m_outputGPUDescriptorHandleSRV; }

private:
    bool LoadShaders(bool throwOnFailure);
    void CreateRootSignatures();
    void CreateRaytracingPipelineObject();
    void CreateShaderBindingTable(Scene& scene);
    void CreateDescriptorHeap();
    void CreateOutputBuffer(uint32_t outputWidth, uint32_t outputHeight);

    ComPtr<ID3D12Device5> m_device;

    ComPtr<ID3D12DescriptorHeap> m_staticDescriptorHeap;

    TextureResource m_outputResource;
    //D3D12_GPU_DESCRIPTOR_HANDLE m_outputGPUDescriptorHandleUAV;
    D3D12_GPU_DESCRIPTOR_HANDLE m_outputGPUDescriptorHandleSRV;

    DynamicConstantBuffer m_frameConstantBuffer;

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
};
