#include "GraphicsResource.h"
#include "ErrorHandling.h"
#include "MathUtils.h"

#include <d3d12.h>
#include "../external/d3dx12.h"

GraphicsResource::GraphicsResource(ID3D12Resource* resource, uint64_t size)
    : m_resource(resource)
    , m_size(size)
{
    //m_resource->AddRef();
}

GraphicsResource::~GraphicsResource()
{
    if (m_resource)
        m_resource->Release();
}

void GraphicsResource::operator=(GraphicsResource&& temp)
{
    this->~GraphicsResource();
    memcpy(this, &temp, sizeof(GraphicsResource));
    temp.m_resource = nullptr;
}

void* GraphicsResource::Map(uint32_t subresource)
{
    void* data;
    ThrowIfFailed(m_resource->Map(subresource, nullptr, &data));
    return data;
}

void GraphicsResource::Unmap(uint32_t subresource)
{
    m_resource->Unmap(subresource, nullptr);
}

GraphicsResource GraphicsResource::CreateBufferForAccellerationStructure(const wchar_t* name, uint64_t size, bool scratch, ID3D12Device5* device)
{
    const D3D12_RESOURCE_STATES initState = scratch ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    size = Align<uint64_t>(size, scratch ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

    ID3D12Resource* buffer;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0),
        initState,
        nullptr, // clear value
        IID_PPV_ARGS(&buffer)));

    buffer->SetName(name);

    return GraphicsResource(buffer, size);
}

GraphicsResource GraphicsResource::CreateUploadHeap(const wchar_t* name, uint64_t size, ID3D12Device5* device)
{
    ID3D12Resource* buffer;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, // clear value
        IID_PPV_ARGS(&buffer)));

    buffer->SetName(name);

    return GraphicsResource(buffer, size);
}
