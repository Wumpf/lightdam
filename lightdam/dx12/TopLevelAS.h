#pragma once

#include "GraphicsResource.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

class BottomLevelAS;

struct BottomLevelASInstance
{
    BottomLevelASInstance(BottomLevelAS* blas) : blas(blas), transform(DirectX::XMMatrixIdentity()) {}

    BottomLevelAS* blas;
    DirectX::XMMATRIX transform;
};

class TopLevelAS
{
public:
    static std::unique_ptr<TopLevelAS> Generate(const std::vector<BottomLevelASInstance>& blasInstances, struct ID3D12GraphicsCommandList4* commandList, struct ID3D12Device5* device);

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const;

private:
    TopLevelAS() = default;

    GraphicsResource m_tlas;
    GraphicsResource m_scratch; // Can be discarded once generation is done since we don't plan on rebuilding.
    GraphicsResource m_descriptors; // Can be discarded?
};
