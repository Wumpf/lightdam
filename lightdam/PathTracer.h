#pragma once

#include "SwapChain.h"
#include "Shader.h"
#include "GraphicsResource.h"
#include "RaytracingShaderBindingTable.h"

struct IDxcBlob;
class Scene;

class PathTracer
{
public:
    PathTracer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight);
    ~PathTracer();

    void SetScene(Scene& scene, ID3D12Device5* device);

    void DrawIteration(ID3D12GraphicsCommandList4* commandList, const TextureResource& renderTarget);


    // todo: resize

private:
    void LoadShaders();
    void CreateLocalRootSignatures(ID3D12Device5* device);
    void CreateRaytracingPipelineObject(ID3D12Device5* device);
    void CreateShaderBindingTable(Scene& scene, ID3D12Device5* device);
    void CreateDescriptorHeap(ID3D12Device5* device);
    void CreateOutputBuffer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight);

    TextureResource m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_rayGenDescriptorHeap;

    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;

    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineObjectProperties;

    RaytracingShaderBindingTable m_shaderBindingTable;

    const uint32_t m_descriptorHeapIncrementSize;
};
