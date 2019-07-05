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

    void ResizeOutput(uint32_t outputWidth, uint32_t outputHeight);
    void SetScene(Scene& scene, ID3D12Device5* device);

    void DrawIteration(ID3D12GraphicsCommandList4* commandList, const TextureResource& renderTarget, const class Camera& activeCamera, int frameIndex);


    // todo: resize

private:
    void LoadShaders();
    void CreateRootSignatures(ID3D12Device5* device);
    void CreateRaytracingPipelineObject(ID3D12Device5* device);
    void CreateShaderBindingTable(Scene& scene, ID3D12Device5* device);
    void CreateDescriptorHeap(ID3D12Device5* device);
    void CreateOutputBuffer(ID3D12Device5* device, uint32_t outputWidth, uint32_t outputHeight);

    TextureResource m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_rayGenDescriptorHeap;
    DynamicConstantBuffer m_frameConstantBuffer;

    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;

    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineObjectProperties;

    RaytracingShaderBindingTable m_shaderBindingTable;

    const uint32_t m_descriptorHeapIncrementSize;
};
