#pragma once

#include "SwapChain.h"

struct IDxcBlob;

class PathTracer
{
public:
    PathTracer();
    ~PathTracer();

private:

    ComPtr<ID3D12Resource> m_outputResource[SwapChain::MaxFramesInFlight];
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    ComPtr<IDxcBlob> m_rayGenLibrary;
    ComPtr<IDxcBlob> m_hitLibrary;
    ComPtr<IDxcBlob> m_missLibrary;
    ComPtr<IDxcBlob> m_shadowLibrary;

    ComPtr<ID3D12RootSignature> m_rayGenSignature;
    ComPtr<ID3D12RootSignature> m_hitSignature;
    ComPtr<ID3D12RootSignature> m_missSignature;
};
