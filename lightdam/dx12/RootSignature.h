#pragma once

#include <wrl/client.h>
using namespace Microsoft::WRL;

ComPtr<struct ID3D12RootSignature> CreateRootSignature(const wchar_t* name, struct ID3D12Device5* device, const struct CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& desc);
