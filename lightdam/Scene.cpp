#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/BottomLevelAS.h"
#include "dx12/SwapChain.h"
#include "ErrorHandling.h"

#include "../external/d3dx12.h"
#include "pbrtParser/Scene.h"

#include <wrl/client.h>
using namespace Microsoft::WRL;

// todo: move
static std::wstring Utf8toUtf16(const std::string& str)
{
    // https://stackoverflow.com/a/26914562

    if (str.empty())
        return L"";

    size_t charsNeeded = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    if (charsNeeded == 0)
        throw std::runtime_error("Failed converting UTF-8 string to UTF-16");

    std::wstring buffer;
    buffer.resize(charsNeeded);
    int charsConverted = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &buffer[0], (int)buffer.size());
    if (charsConverted == 0)
        throw std::runtime_error("Failed converting UTF-8 string to UTF-16");

    return buffer;
}

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

static void CreateTestTriangle(Scene::Mesh& mesh, ID3D12Device5* device)
{
    // Define the geometry for a triangle.
    Vertex triangleVertices[] =
    {
        { { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };

    // TODO: Don't use upload heaps
    const UINT vertexBufferSize = sizeof(triangleVertices);
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

std::unique_ptr<Scene> Scene::LoadTestScene(SwapChain& swapChain, ID3D12Device5* device)
{
    auto scene = std::unique_ptr<Scene>(new Scene());
    scene->m_meshes.resize(1);
    CreateTestTriangle(scene->m_meshes.back(), device);
    scene->m_originFilePath = "Builtin Test Scene";
    scene->CreateAccellerationDataStructure(swapChain, device);
    return scene;
}

std::unique_ptr<Scene> Scene::LoadPbrtScene(const std::string& pbrtFilePath, SwapChain& swapChain, ID3D12Device5* device)
{
    pbrt::Scene::SP pbrtScene;
    try
    {
        pbrtScene = pbrt::importPBRT(pbrtFilePath);
    }
    catch (std::exception& exception)
    {
        std::cout << "Failed to load scene: " << exception.what() << std::endl;
    }

    if (!pbrtScene)
        return nullptr;
    pbrtScene->makeSingleLevel();


    auto scene = std::unique_ptr<Scene>(new Scene());
    scene->m_originFilePath = pbrtFilePath;

    for (const pbrt::Instance::SP& instance : pbrtScene->world->instances)
    {
        for (const pbrt::Shape::SP& shape : instance->object->shapes)
        {
            // todo: Handle material & textures
            //shape->material
            const auto triangleShape = shape->as<pbrt::TriangleMesh>();
            if (!triangleShape)
                continue;

            // TODO: Don't use upload heaps
            Mesh mesh;
            mesh.vertexBuffer = GraphicsResource::CreateUploadHeap(Utf8toUtf16(instance->object->name + " VB").c_str(), sizeof(Vertex) * triangleShape->vertex.size(), device);
            mesh.vertexCount = (uint32_t)triangleShape->vertex.size();
            uint64_t indexbufferSize = sizeof(uint32_t) * triangleShape->index.size() * 3;
            mesh.indexBuffer = GraphicsResource::CreateUploadHeap(Utf8toUtf16(instance->object->name + " IB").c_str(), indexbufferSize, device);
            mesh.indexCount = (uint32_t)triangleShape->index.size() * 3;
            {
                ScopedResourceMap vertexBufferData(mesh.vertexBuffer);
                Vertex* vertexData = (Vertex*)vertexBufferData.Get();
                for (size_t vertexIdx = 0; vertexIdx < triangleShape->vertex.size(); ++vertexIdx)
                {
                    auto position = instance->xfm * triangleShape->vertex[vertexIdx];
                    vertexData[vertexIdx].position.x = position.x;
                    vertexData[vertexIdx].position.y = position.y;
                    vertexData[vertexIdx].position.z = position.z;

                    if (!triangleShape->texcoord.empty())
                    {
                        vertexData[vertexIdx].color.x = triangleShape->texcoord[vertexIdx].x;
                        vertexData[vertexIdx].color.y = triangleShape->texcoord[vertexIdx].y;
                        vertexData[vertexIdx].color.z = 0;
                        vertexData[vertexIdx].color.w = 0;
                    }
                    else
                        memset(&vertexData[vertexIdx].color, 0, sizeof(vertexData[vertexIdx].color));
                }
            }
            {
                ScopedResourceMap indexBufferData(mesh.indexBuffer);
                memcpy(indexBufferData.Get(), triangleShape->index.data(), indexbufferSize);
            }
            scene->m_meshes.push_back(std::move(mesh));
        }
    }

    // todo: be clever about BLAS/TLAS instances

    //scene->m_meshes.resize(1);
    //CreateTestTriangle(scene->m_meshes.back(), device);

    scene->CreateAccellerationDataStructure(swapChain, device);

    return scene;
}

void Scene::CreateAccellerationDataStructure(SwapChain& swapChain, ID3D12Device5* device)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ComPtr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    std::vector<BottomLevelASMesh> blasMeshes(m_meshes.size());
    for (int i = 0; i < m_meshes.size(); ++i)
    {
        blasMeshes[i].vertexBuffer = { m_meshes[i].vertexBuffer->GetGPUVirtualAddress(), sizeof(Vertex) };
        blasMeshes[i].vertexCount = m_meshes[i].vertexCount;
        blasMeshes[i].indexBuffer = m_meshes[i].indexBuffer->GetGPUVirtualAddress();
        blasMeshes[i].indexCount = m_meshes[i].indexCount;
    }
    auto blas = BottomLevelAS::Generate(blasMeshes, commandList.Get(), device);

    auto instance = BottomLevelASInstance(blas.get());
    m_tlas = TopLevelAS::Generate({ instance }, commandList.Get(), device);
    m_blas.push_back(std::move(blas));

    // todo: Don't use the swapChain for this, but an independent compute queue.
    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList.Get() };
    swapChain.GetGraphicsCommandQueue()->ExecuteCommandLists(1, commandLists);
    swapChain.WaitUntilGraphicsQueueProcessingDone();
}

Scene::Scene()
{
}

Scene::~Scene()
{
}
