#pragma once

#include <cstdint>
#include <cstring>
#include <utility>
#include <d3d12.h>

struct ID3D12Device5;
struct ID3D12Resource;

struct D3D12_RANGE;

class GraphicsResource
{
public:
    GraphicsResource() = default;
    GraphicsResource(ID3D12Resource* resource, uint64_t size);
    ~GraphicsResource();

    GraphicsResource(GraphicsResource&& temp) { *this = std::move(temp); }
    void operator = (GraphicsResource&& temp);

    GraphicsResource(const GraphicsResource&) = delete;
    void operator = (const GraphicsResource&) = delete;

    ID3D12Resource* Get() const { return m_resource; }
    ID3D12Resource* operator ->() const { return m_resource; }

    void* Map(uint32_t subresource = 0);
    void Unmap(uint32_t subresource = 0);

    static GraphicsResource CreateBufferForRTAccellerationStructure(const wchar_t* name, uint64_t size, bool scratch, ID3D12Device5* device);
    static GraphicsResource CreateUploadHeap(const wchar_t* name, uint64_t size, ID3D12Device5* device);


protected:
    ID3D12Resource* m_resource = nullptr;
    uint64_t m_sizeInBytes = 0;
};

class TextureResource : public GraphicsResource
{
public:
    TextureResource() = default;
    TextureResource(ID3D12Resource* resource, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mipLevel);

    TextureResource(TextureResource&& temp) { *this = std::move(temp); }
    void operator = (TextureResource&& temp);

    static TextureResource CreateTexture2D(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mipLevels, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, ID3D12Device5* device);

    DXGI_FORMAT GetFormat() const      { return m_format; }
    uint32_t    GetWidth() const       { return m_width; }
    uint32_t    GetHeight() const      { return m_height; }
    uint32_t    GetMipLevels() const   { return m_mipLevels; }

private:
    DXGI_FORMAT m_format;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
};


class ScopedResourceMap
{
public:
    ScopedResourceMap(GraphicsResource& resource, uint32_t subresource = 0)
        : m_resource(resource)
        , m_subresource(subresource)
        , m_data(m_resource.Map(subresource))
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

uint32_t GetBitsPerPixel(DXGI_FORMAT fmt);
