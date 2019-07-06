#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/BottomLevelAS.h"
#include "dx12/SwapChain.h"
#include "ErrorHandling.h"
#include "../external/d3dx12.h"

#include <wrl/client.h>
using namespace Microsoft::WRL;

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

std::unique_ptr<Scene> Scene::LoadScene(SwapChain& swapChain, ID3D12Device5* device)
{
    auto scene = std::unique_ptr<Scene>(new Scene());

    // C:\dev\current_development\pbrt-v3-scenes\killeroos\killeroo-simple.pbrt


    // Testtriangle
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        // TODO: Don't use upload heaps
        scene->m_meshes.resize(1);
        const UINT vertexBufferSize = sizeof(triangleVertices);
        Scene::Mesh& mesh = scene->m_meshes.back();
        mesh.vertexBuffer = GraphicsResource::CreateUploadHeap(L"Triangle VB", sizeof(triangleVertices), device);
        mesh.vertexCount = 3;
        mesh.indexBuffer = GraphicsResource::CreateUploadHeap(L"Triangle IB", sizeof(int32_t) * 3, device);
        mesh.indexCount = 3;

        {
            ScopedResourceMap vertexBufferData(mesh.vertexBuffer);
            memcpy(vertexBufferData.Get(), triangleVertices, sizeof(triangleVertices));
        }
        {
            ScopedResourceMap indexBufferData(mesh.indexBuffer);
            int32_t* indices = (int32_t*)indexBufferData.Get();
            indices[0] = 2;
            indices[1] = 1;
            indices[2] = 0;
        }
    }

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ComPtr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    std::vector<BottomLevelASMesh> blasMeshes(scene->m_meshes.size());
    for (int i = 0; i < scene->m_meshes.size(); ++i)
    {
        blasMeshes[i].vertexBuffer = { scene->m_meshes[i].vertexBuffer->GetGPUVirtualAddress(), sizeof(Vertex) };
        blasMeshes[i].vertexCount = scene->m_meshes[i].vertexCount;
        blasMeshes[i].indexBuffer = scene->m_meshes[i].indexBuffer->GetGPUVirtualAddress();
        blasMeshes[i].indexCount = scene->m_meshes[i].indexCount;
    }
    auto blas = BottomLevelAS::Generate(blasMeshes, commandList.Get(), device);

    auto instance = BottomLevelASInstance(blas.get());
    scene->m_tlas = TopLevelAS::Generate({ instance }, commandList.Get(), device);
    scene->m_blas.push_back(std::move(blas));

    // todo: Don't use the swapChain for this, but an independent compute queue.
    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList.Get() };
    swapChain.GetGraphicsCommandQueue()->ExecuteCommandLists(1, commandLists);
    swapChain.WaitUntilGraphicsQueueProcessingDone();

    return scene;
}

Scene::Scene()
{
}

Scene::~Scene()
{
}
