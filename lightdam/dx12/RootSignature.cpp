#include "RootSignature.h"
#include <d3d12.h>
#include "../../external/d3dx12.h"
#include "../ErrorHandling.h"

ComPtr<ID3D12RootSignature> CreateRootSignature(const wchar_t* name, ID3D12Device5* device, const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& desc)
{
    ComPtr<ID3DBlob> sigBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, nullptr));
    ComPtr<ID3D12RootSignature> signature;
    ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&signature)));
    signature->SetName(name);
    return signature;
}
