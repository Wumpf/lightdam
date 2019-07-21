#pragma once

#include "dx12/Shader.h"

#include <wrl/client.h>
using namespace Microsoft::WRL;

class TextureResource;

class ToneMapper
{
public:
    ToneMapper(ID3D12Device5* device);
    ~ToneMapper();

    void ReloadShaders();
    void Draw(ID3D12GraphicsCommandList4* commandList, const D3D12_GPU_DESCRIPTOR_HANDLE& inputTextureDescHandle);

private:
    void CreateRootSignature();
    void CreatePipelineState();
    bool LoadShaders(bool throwOnFailure);

    ComPtr<ID3D12Device5> m_device;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    Shader m_vertexShader;
    Shader m_pixelShader;
};
