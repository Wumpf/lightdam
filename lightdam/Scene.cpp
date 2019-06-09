#include "Scene.h"
#include "TopLevelAS.h"
#include "BottomLevelAS.h"
#include "SwapChain.h"
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

    // Testtriangle
    GraphicsResource vertexBuffer;
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);
        vertexBuffer = GraphicsResource::CreateUploadHeap(sizeof(triangleVertices), device);

        void* vertexBufferData;
        ThrowIfFailed(vertexBuffer->Map(0, nullptr, &vertexBufferData));
        memcpy(vertexBufferData, triangleVertices, sizeof(triangleVertices));
        vertexBuffer->Unmap(0, nullptr);
    }

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ComPtr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    BottomLevelASMesh mesh;
    mesh.vertexBuffer = { vertexBuffer->GetGPUVirtualAddress(), sizeof(Vertex) };
    mesh.vertexCount = 3;
    auto blas = BottomLevelAS::Generate({ mesh }, commandList.Get(), device);

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
