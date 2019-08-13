#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/BottomLevelAS.h"
#include "dx12/SwapChain.h"
#include "ErrorHandling.h"
#include "StringConversion.h"

#include "../external/d3dx12.h"
#include "pbrtParser/Scene.h"

#include <fstream>

#include <wrl/client.h>
using namespace Microsoft::WRL;

static DirectX::XMFLOAT3 PbrtVec3ToXMFloat3(pbrt::vec3f v)
{
    return DirectX::XMFLOAT3{ v.x, v.y, v.z };
}

static DirectX::XMVECTOR PbrtVec3ToXMVector(pbrt::vec3f v)
{
    return DirectX::XMLoadFloat3(&PbrtVec3ToXMFloat3(v));
}

static void CreateTestTriangle(Scene::Mesh& mesh, ID3D12Device5* device, DirectX::XMFLOAT3 offset, DirectX::XMFLOAT3 normal)
{
    // Define the geometry for a triangle.
    Scene::Vertex triangleVertices[] =
    {
        { { 0.0f + offset.x, 0.25f + offset.y, offset.z }, normal },
        { { 0.25f + offset.x, -0.25f + offset.y, offset.z }, normal },
        { { -0.25f + offset.x, -0.25f + offset.y, offset.z }, normal }
    };

    // TODO: Don't use upload heaps
    const UINT vertexBufferSize = sizeof(triangleVertices);
    mesh.vertexBuffer = GraphicsResource::CreateUploadHeap(L"Triangle VB", sizeof(triangleVertices), device);
    mesh.vertexCount = 3;
    mesh.indexBuffer = GraphicsResource::CreateUploadHeap(L"Triangle IB", sizeof(int32_t) * 3, device);
    mesh.indexCount = 3;
    mesh.constantBuffer = GraphicsResource::CreateUploadHeap(L"Triangle CB", sizeof(Scene::MeshConstants), device);

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
    {
        ScopedResourceMap contextBufferData(mesh.constantBuffer);
        auto constants = (Scene::MeshConstants*)contextBufferData.Get();
        constants->MeshIndex = 0;
        constants->Diffuse = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    }
}

std::unique_ptr<Scene> Scene::LoadTestScene(SwapChain& swapChain, ID3D12Device5* device)
{
    auto scene = std::unique_ptr<Scene>(new Scene());
    scene->m_meshes.resize(3);
    CreateTestTriangle(scene->m_meshes[0], device, DirectX::XMFLOAT3(-0.5f, 0.0f, 0.0f), { 1.0f, 0.0f, 0.0f });
    CreateTestTriangle(scene->m_meshes[1], device, DirectX::XMFLOAT3(0.5f, 0.0f, 0.0f), { 0.0f, 1.0f, 0.0f });
    CreateTestTriangle(scene->m_meshes[2], device, DirectX::XMFLOAT3(0.0f, 0.5f, 0.0f), { 0.0f, 0.0f, 1.0f });
    scene->m_originFilePath = "Builtin Test Scene";
    scene->CreateAccellerationDataStructure(swapChain, device);
    return scene;
}

static void AddAreaLights(Scene::Vertex* vertices, uint32_t* indices, uint32_t numTriangles, DirectX::SimpleMath::Vector3 emittedRadiance, std::vector<Scene::AreaLightTriangle>& outAreaLights)
{
    const auto firstNewAreaLightIdx = outAreaLights.size();
    outAreaLights.resize(outAreaLights.size() + numTriangles);

    float totalArea = 0.0f;
    for (uint32_t triangleIdx = 0; triangleIdx < numTriangles; ++triangleIdx)
    {
        Scene::AreaLightTriangle& areaLightTriangle = outAreaLights[firstNewAreaLightIdx + triangleIdx];
        areaLightTriangle.positions[0] = vertices[indices[triangleIdx * 3 + 0]].position;
        areaLightTriangle.positions[1] = vertices[indices[triangleIdx * 3 + 1]].position;
        areaLightTriangle.positions[2] = vertices[indices[triangleIdx * 3 + 2]].position;
        areaLightTriangle.normals[0] = vertices[indices[triangleIdx * 3 + 0]].normal;
        areaLightTriangle.normals[1] = vertices[indices[triangleIdx * 3 + 1]].normal;
        areaLightTriangle.normals[2] = vertices[indices[triangleIdx * 3 + 2]].normal;
        areaLightTriangle.area = (areaLightTriangle.positions[1] - areaLightTriangle.positions[0]).Cross(areaLightTriangle.positions[2] - areaLightTriangle.positions[0]).Length() * 0.5f;
        areaLightTriangle.emittedRadiance = emittedRadiance;
        totalArea += areaLightTriangle.area;
    }
}

static Scene::Mesh LoadPbrtMesh(uint32_t index, const pbrt::TriangleMesh::SP& triangleShape, const pbrt::Instance::SP& instance, ID3D12Device5* device, std::vector<Scene::AreaLightTriangle>& outAreaLights)
{
    // TODO: Don't use upload heaps
    // todo: Handle material & textures
     //shape->material
    Scene::Mesh mesh;
    mesh.vertexBuffer = GraphicsResource::CreateUploadHeap(Utf8toUtf16(instance->object->name + " VB").c_str(), sizeof(Scene::Vertex) * triangleShape->vertex.size(), device);
    mesh.vertexCount = (uint32_t)triangleShape->vertex.size();
    uint64_t indexbufferSize = sizeof(uint32_t) * triangleShape->index.size() * 3;
    mesh.indexBuffer = GraphicsResource::CreateUploadHeap(Utf8toUtf16(instance->object->name + " IB").c_str(), indexbufferSize, device);
    mesh.indexCount = (uint32_t)triangleShape->index.size() * 3;
    mesh.constantBuffer = GraphicsResource::CreateUploadHeap(Utf8toUtf16(instance->object->name + " CB").c_str(), sizeof(Scene::MeshConstants), device);
    {
        // Generate triangles on the shape, so we can use the binary format of the pbrt library next time.
        if (triangleShape->normal.empty())
        {
            triangleShape->normal.resize(triangleShape->vertex.size());
            memset(triangleShape->normal.data(), 0, sizeof(pbrt::vec3f) * triangleShape->normal.size());

            for (auto triangle : triangleShape->index)
            {
                auto v1 = triangleShape->vertex[triangle.x];
                auto v2 = triangleShape->vertex[triangle.y];
                auto v3 = triangleShape->vertex[triangle.z];
                auto triangleNormal = pbrt::math::cross(v1 - v3, v2 - v3);
                triangleShape->normal[triangle.x] = triangleShape->normal[triangle.x] + triangleNormal;
                triangleShape->normal[triangle.y] = triangleShape->normal[triangle.y] + triangleNormal;
                triangleShape->normal[triangle.z] = triangleShape->normal[triangle.z] + triangleNormal;
            }
            for (auto& normal : triangleShape->normal)
                normal = pbrt::math::normalize(normal);
        }

        auto normalTransformation = pbrt::math::inverse_transpose(instance->xfm.l);
        std::vector<Scene::Vertex> vertices(triangleShape->vertex.size());
        for (size_t vertexIdx = 0; vertexIdx < triangleShape->vertex.size(); ++vertexIdx)
        {
            vertices[vertexIdx].position = PbrtVec3ToXMFloat3(instance->xfm * triangleShape->vertex[vertexIdx]);
            auto normal = normalTransformation * triangleShape->normal[vertexIdx];
            if (triangleShape->reverseOrientation) normal = -normal;
            vertices[vertexIdx].normal = PbrtVec3ToXMFloat3(normal);
        }

        {
            ScopedResourceMap vertexBufferData(mesh.vertexBuffer);
            memcpy(vertexBufferData.Get(), vertices.data(), sizeof(Scene::Vertex) * triangleShape->vertex.size());
        }

        pbrt::DiffuseAreaLightRGB::SP areaLight = triangleShape->areaLight ? triangleShape->areaLight->as<pbrt::DiffuseAreaLightRGB>() : nullptr;
        if (areaLight)
            AddAreaLights(vertices.data(), (uint32_t*)triangleShape->index.data(), (uint32_t)triangleShape->index.size(), PbrtVec3ToXMFloat3(areaLight->L), outAreaLights);
    }
    {
        ScopedResourceMap indexBufferData(mesh.indexBuffer);
        memcpy(indexBufferData.Get(), triangleShape->index.data(), indexbufferSize);
    }
    {
        ScopedResourceMap contextBufferData(mesh.constantBuffer);
        auto constants = (Scene::MeshConstants*)contextBufferData.Get();
        constants->MeshIndex = index;
        const auto matteMaterial = triangleShape->material->as<pbrt::MatteMaterial>();
        if (triangleShape->material)
            constants->Diffuse = DirectX::XMFLOAT3(matteMaterial->kd.x, matteMaterial->kd.y, matteMaterial->kd.z);
        else
            constants->Diffuse = DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f);
    }

    return mesh;
}

std::unique_ptr<Scene> Scene::LoadPbrtScene(const std::string& pbrtFilePath, SwapChain& swapChain, ID3D12Device5* device)
{
    pbrt::Scene::SP pbrtScene;

    std::string pbfFilePath = pbrtFilePath.substr(0, pbrtFilePath.find_last_of('.')) + ".pbf";
    bool pbfFileExists = std::ifstream(pbfFilePath.c_str()).good();

    if (pbfFileExists)
    {
        try
        {
            pbrtScene = pbrt::Scene::loadFrom(pbfFilePath);
        }
        catch (std::exception& exception)
        {
            std::cout << "Failed to load scene from pbf: " << exception.what() << std::endl;
        }
    }
    else
    {
        try
        {
            pbrtScene = pbrt::importPBRT(pbrtFilePath);
        }
        catch (std::exception& exception)
        {
            std::cout << "Failed to load scene from pbrt: " << exception.what() << std::endl;
        }
    }

    if (!pbrtScene)
        return nullptr;
    pbrtScene->makeSingleLevel();

    auto scene = std::unique_ptr<Scene>(new Scene());
    scene->m_originFilePath = pbrtFilePath;
    if (pbrtScene->film)
    {
        scene->m_screenHeight = (uint32_t)pbrtScene->film->resolution.x;
        scene->m_screenWidth = (uint32_t)pbrtScene->film->resolution.y;
    }

    for (const auto& pbrtCamera : pbrtScene->cameras)
    {
        scene->m_cameras.emplace_back();
        auto& camera = scene->m_cameras.back();
        camera.SetPosition(PbrtVec3ToXMVector(pbrtCamera->frame.p));
        camera.SetUp(DirectX::XMVector3Normalize(PbrtVec3ToXMVector(pbrtCamera->frame.l.vy)));
        camera.SetDirection(DirectX::XMVector3Normalize(PbrtVec3ToXMVector(pbrtCamera->frame.l.vz)));
        camera.SetVFovRad(pbrtCamera->fov * ((float)M_PI / 180.0f));
        camera.SnapUpToAxis(); // Makes camera easier to control
    }

    for (const pbrt::Instance::SP& instance : pbrtScene->world->instances)
    {
        for (const pbrt::Shape::SP& shape : instance->object->shapes)
        {
            const auto triangleShape = shape->as<pbrt::TriangleMesh>();
            if (triangleShape)
                scene->m_meshes.push_back(LoadPbrtMesh((uint32_t)scene->m_meshes.size(), triangleShape, instance, device, scene->m_areaLights));
        }
        for (const pbrt::LightSource::SP& lightSource : instance->object->lightSources)
        {
            // todo.
        }
    }

    if (!pbfFileExists)
        pbrtScene->saveTo(pbfFilePath);


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

const std::string Scene::GetName() const
{
    auto lastSlash = m_originFilePath.find_last_of("/\\");
    auto lastDot = m_originFilePath.find_last_of('.');
    return m_originFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

Scene::Scene()
{
}

Scene::~Scene()
{
}
