#pragma once

#include "SwapChain.h"
#include "Shader.h"

struct IDxcBlob;

class PathTracer
{
public:
    PathTracer(ID3D12Device5* device);
    ~PathTracer();

    void SetScene(class Scene& scene);

private:
    void LoadShaders();
    void CreateLocalRootSignatures(ID3D12Device5* device);
    void CreateRaytracingPipelineObject(ID3D12Device5* device);
    void CreateShaderBindingTable();

    ComPtr<ID3D12Resource> m_outputResource[SwapChain::MaxFramesInFlight];
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;

    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;

    ComPtr<ID3D12StateObject> m_raytracingPipelineObject;
};
