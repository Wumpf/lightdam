#include "ToneMapper.h"
#include "../external/d3dx12.h"
#include "dx12/RootSignature.h"
#include "dx12/GraphicsResource.h"
#include "ErrorHandling.h"

ToneMapper::ToneMapper(ID3D12Device5* device)
    : m_device(device)
{
    LoadShaders(true);
    CreateRootSignature();
    CreatePipelineState();
}

ToneMapper::~ToneMapper()
{
}

void ToneMapper::CreateRootSignature()
{
    CD3DX12_ROOT_PARAMETER1 params[1] = {};
    CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] =
    {
        CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE),  // Output buffer in t0,space0
    };
    params[0].InitAsDescriptorTable(_countof(descriptorRanges), descriptorRanges, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_STATIC_SAMPLER_DESC pointSampler;
    pointSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    pointSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    pointSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    pointSampler.MipLODBias = 0;
    pointSampler.MaxAnisotropy = 0;
    pointSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    pointSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    pointSampler.MinLOD = 0.0f;
    pointSampler.MaxLOD = D3D12_FLOAT32_MAX;
    pointSampler.ShaderRegister = 0;
    pointSampler.RegisterSpace = 0;
    pointSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(params), params);
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = &pointSampler;
    m_rootSignature = ::CreateRootSignature(L"ToneMapperRootSignature", m_device.Get(), rootSignatureDesc);
}

void ToneMapper::CreatePipelineState()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.NodeMask = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    psoDesc.VS = m_vertexShader.GetByteCode();
    psoDesc.PS = m_pixelShader.GetByteCode();

    // blend state
    {
        D3D12_BLEND_DESC& desc = psoDesc.BlendState;
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    // rasterizer state
    {
        D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        desc.CullMode = D3D12_CULL_MODE_NONE;
    }

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));
}

void ToneMapper::ReloadShaders()
{
    if (LoadShaders(false))
        CreatePipelineState();
}

bool ToneMapper::LoadShaders(bool throwOnFailure)
{
    const Shader::LoadInstruction shaderLoads[] =
    {
        { Shader::Type::Vertex, L"shaders/ToneMap.hlsl", L"FullScreenTriangle" },
        { Shader::Type::Pixel, L"shaders/ToneMap.hlsl", L"ToneMap" },
    };
    Shader* shaders[] =
    {
        &m_vertexShader,
        &m_pixelShader,
    };
    return Shader::ReplaceShadersOnSuccessfulCompileFromFiles(shaderLoads, shaders, throwOnFailure);
}

void ToneMapper::Draw(ID3D12GraphicsCommandList4* commandList, const D3D12_GPU_DESCRIPTOR_HANDLE& inputTextureDescHandle)
{
    // Assumes descriptor heap with input texture is already set.
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetGraphicsRootDescriptorTable(0, inputTextureDescHandle);
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
