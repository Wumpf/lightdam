#pragma once

#include "SwapChain.h"
#include "Shader.h"
#include "GraphicsResource.h"

struct IDxcBlob;
class Scene;

class PathTracer
{
public:
    PathTracer(ID3D12Device5* device);
    ~PathTracer();

    void SetScene(Scene& scene, ID3D12Device5* device);

private:
    void LoadShaders();
    void CreateLocalRootSignatures(ID3D12Device5* device);
    void CreateRaytracingPipelineObject(ID3D12Device5* device);
    void CreateShaderBindingTable(Scene& scene, ID3D12Device5* device);
    void CreateSceneIndependentShaderResources(ID3D12Device5* device);

    ComPtr<ID3D12Resource> m_outputResource[SwapChain::MaxFramesInFlight];
    ComPtr<ID3D12DescriptorHeap> m_rayGenDescriptorHeap;

    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;

    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineObjectProperties;

    GraphicsResource m_shaderBindingTable;
};
