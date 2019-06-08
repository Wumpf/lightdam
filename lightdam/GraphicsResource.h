#pragma once

#include <cstdint>
#include <cstring>
#include <utility>

struct ID3D12Device5;
struct ID3D12Resource;

class GraphicsResource
{
public:
    GraphicsResource() = default;
    GraphicsResource(ID3D12Resource* resource, uint64_t size);
    ~GraphicsResource();

    GraphicsResource(GraphicsResource&& temp) { *this = std::move(temp); }
    void operator = (GraphicsResource&& temp) { memcpy(this, &temp, sizeof(GraphicsResource)); temp.m_resource = nullptr; }

    GraphicsResource(const GraphicsResource&) = delete;
    void operator = (const GraphicsResource&) = delete;

    ID3D12Resource* Get() const { return m_resource; }
    ID3D12Resource* operator ->() const { return m_resource; }


    static GraphicsResource CreateBufferForAccellerationStructure(uint64_t size, bool scratch, ID3D12Device5* device);
    static GraphicsResource CreateUploadHeap(uint64_t size, ID3D12Device5* device);

private:
    ID3D12Resource* m_resource = nullptr;
    uint64_t m_size = 0;
};
