#pragma once

#include "dx12/SwapChain.h"
#include "dx12/Shader.h"
#include "dx12/GraphicsResource.h"
#include "dx12/DynamicConstantBuffer.h"
#include "dx12/RaytracingShaderBindingTable.h"

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

    void DrawIteration(ID3D12GraphicsCommandList4* commandList, const TextureResource& renderTarget, const class Camera& activeCamera, int frameIndex);


    // todo: resize

private:
    bool LoadShaders(bool throwOnFailure);
    void CreateRootSignatures();
    void CreateRaytracingPipelineObject();
    void CreateShaderBindingTable(Scene& scene);
    void CreateDescriptorHeap();
    void CreateOutputBuffer(uint32_t outputWidth, uint32_t outputHeight);

    ComPtr<ID3D12Device5> m_device;

    TextureResource m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_staticDescriptorHeap;
    DynamicConstantBuffer m_frameConstantBuffer;

    struct PathTracerShaders
    {
        Shader rayGenLibrary;
        Shader hitLibrary;
        Shader missLibrary;
        Shader shadowRayLibrary;
    };
    PathTracerShaders m_shaders;

    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12RootSignature> m_signatureEmpty;
    ComPtr<ID3D12RootSignature> m_signatureSceneData;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineObjectProperties;

    RaytracingBindingTableGenerator m_bindingTableGenerator;
    RaytracingShaderBindingTable m_shaderBindingTable;

    const uint32_t m_descriptorHeapIncrementSize;
};
