#include "GraphicsResource.h"
#include "../ErrorHandling.h"
#include "../MathUtils.h"

#include <d3d12.h>
#include "../../external/d3dx12.h"

GraphicsResource::GraphicsResource(const wchar_t* name, ID3D12Resource* resource)
    : m_resource(resource)
    , m_desc(resource ? resource->GetDesc() : D3D12_RESOURCE_DESC{})
    , m_name(name)
{
    m_resource->AddRef();
    m_resource->SetName(name);
}

GraphicsResource::GraphicsResource(const wchar_t* name, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, const D3D12_RESOURCE_DESC& desc, ID3D12Device* device)
    : m_desc(desc)
    , m_name(name)
{
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(heapType),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        nullptr, // clear value
        IID_PPV_ARGS(&m_resource)));

    m_resource->SetName(name);
}

void GraphicsResource::Release()
{
    if (!m_resource)
        return;
    m_resource->Release();
    m_resource = nullptr;
}

void GraphicsResource::operator=(GraphicsResource&& temp)
{
    Release();
    m_resource = temp.m_resource;
    m_desc = std::move(temp.m_desc);
    m_name = std::move(temp.m_name);
    temp.m_resource = nullptr;
}

void GraphicsResource::operator = (const GraphicsResource& cpy)
{
    Release();
    m_resource = cpy.m_resource;
    m_desc = cpy.m_desc;
    m_name = cpy.m_name;
    m_resource->AddRef();
}

void* GraphicsResource::Map(bool writeonly, uint32_t subresource)
{
    void* data;

    CD3DX12_RANGE readRange(0, 0);
    if (!writeonly)
        readRange.End = GetSizeInBytes();

    ThrowIfFailed(m_resource->Map(subresource, &readRange, &data));
    return data;
}

void GraphicsResource::Unmap(uint32_t subresource)
{
    if (m_resource)
        m_resource->Unmap(subresource, nullptr);
}

GraphicsResource GraphicsResource::CreateBufferForRTAccellerationStructure(const wchar_t* name, uint64_t size, bool scratch, ID3D12Device* device)
{
    const D3D12_RESOURCE_STATES initState = scratch ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    size = Align<uint64_t>(size, scratch ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);

    return GraphicsResource(name, D3D12_HEAP_TYPE_DEFAULT, initState, desc, device);
}

GraphicsResource GraphicsResource::CreateUploadBuffer(const wchar_t* name, uint64_t size, ID3D12Device* device)
{
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
    return GraphicsResource(name, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, device);
}

GraphicsResource GraphicsResource::CreateReadbackBuffer(const wchar_t* name, uint64_t size, ID3D12Device* device)
{
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
    return GraphicsResource(name, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, desc, device);
}

uint64_t GraphicsResource::GetSizeInBytes() const
{ 
    return m_desc.Width * m_desc.Height * m_desc.DepthOrArraySize *
        (m_desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetBitsPerPixel(m_desc.Format) / 8);
}

TextureResource TextureResource::CreateTexture2D(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mipLevels, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, ID3D12Device* device)
{
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, mipLevels, 1, 0, flags);
    return TextureResource(name, D3D12_HEAP_TYPE_DEFAULT, initialState, desc, device);
}

uint32_t GetBitsPerPixel(DXGI_FORMAT fmt)
{
    switch (static_cast<int>(fmt))
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;

    default:
        throw std::exception("Unknown DXGI format");
        return 0;
    }
}