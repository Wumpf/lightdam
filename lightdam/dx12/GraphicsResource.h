#pragma once

#include <cstdint>
#include <cstring>
#include <utility>
#include <d3d12.h>
#include <string>

struct ID3D12Device;
struct ID3D12Resource;

struct D3D12_RANGE;

uint32_t GetBitsPerPixel(DXGI_FORMAT fmt);

class GraphicsResource
{
public:
    GraphicsResource() = default;
    GraphicsResource(const wchar_t* name, ID3D12Resource* resource);
    GraphicsResource(const wchar_t* name, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, const D3D12_RESOURCE_DESC& desc, ID3D12Device* device);
    virtual ~GraphicsResource() { Release(); }

    GraphicsResource(GraphicsResource&& temp) { *this = std::move(temp); }
    void operator = (GraphicsResource&& temp);

    GraphicsResource(const GraphicsResource& cpy) { *this = cpy; }
    void operator = (const GraphicsResource& cpy);

    void Release();

    ID3D12Resource* Get() const { return m_resource; }
    ID3D12Resource* operator ->() const { return m_resource; }

    // Write only mapping
    void* Map(bool writeonly = true, uint32_t subresource = 0);
    void Unmap(uint32_t subresource = 0);

    static GraphicsResource CreateBufferForRTAccellerationStructure(const wchar_t* name, uint64_t size, bool scratch, ID3D12Device* device);
    static GraphicsResource CreateStaticBuffer(const wchar_t* name, uint64_t size, ID3D12Device* device);
    static GraphicsResource CreateUploadBuffer(const wchar_t* name, uint64_t size, ID3D12Device* device);
    static GraphicsResource CreateReadbackBuffer(const wchar_t* name, uint64_t size, ID3D12Device* device);

    uint64_t GetSizeInBytes() const;
    const std::wstring& GetName() const { return m_name; }

protected:
    ID3D12Resource* m_resource = nullptr;
    D3D12_RESOURCE_DESC m_desc = {};
    std::wstring m_name;
};

class TextureResource : public GraphicsResource
{
public:
    TextureResource() = default;
    using GraphicsResource::GraphicsResource;

    TextureResource(TextureResource&& temp) : GraphicsResource(std::move(temp)) {}
    void operator = (TextureResource&& temp)  { GraphicsResource::operator=(std::move(temp)); }

    static TextureResource CreateTexture2D(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mipLevels, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, ID3D12Device* device);

    TextureResource CaptureTexture(ID3D12CommandList* commandList, UINT64 srcPitch, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState, ID3D12Device* device);

    DXGI_FORMAT GetFormat() const      { return m_desc.Format; }
    uint32_t    GetWidth() const       { return (uint32_t)m_desc.Width; }
    uint32_t    GetHeight() const      { return (uint32_t)m_desc.Height; }
    uint16_t    GetMipLevels() const   { return m_desc.MipLevels; }
};


class ScopedResourceMap
{
public:
    ScopedResourceMap(GraphicsResource& resource, bool writeonly = true, uint32_t subresource = 0)
        : m_resource(resource)
        , m_subresource(subresource)
        , m_data(m_resource.Map(writeonly, subresource))
    {}
    ScopedResourceMap(ScopedResourceMap&& temp)
        : m_resource(temp.m_resource)
        , m_subresource(temp.m_subresource)
        , m_data(temp.m_data)
    {
        temp.m_data = nullptr;
    }
    ~ScopedResourceMap() { Unmap(); }

    ScopedResourceMap(const ScopedResourceMap&) = delete;
    void operator = (const ScopedResourceMap&) = delete;

    void* Get() { return m_data; }
    void Unmap() { if (m_data) m_resource.Unmap(); m_data = nullptr; }

private:
    GraphicsResource& m_resource;
    uint32_t m_subresource;
    void* m_data;
};
