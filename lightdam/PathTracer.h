#pragma once

#include "SwapChain.h"
#include "Shader.h"

struct IDxcBlob;

class PathTracer
{
public:
    PathTracer();
    ~PathTracer();

    void SetScene(class Scene& scene);

private:
    void LoadShaders();
    void CreateRaytracingPipelineObject();
    void CreateShaderBindingTable();

    ComPtr<ID3D12Resource> m_outputResource[SwapChain::MaxFramesInFlight];
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    Shader m_rayGenLibrary;
    Shader m_hitLibrary;
    Shader m_missLibrary;

    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;
};
