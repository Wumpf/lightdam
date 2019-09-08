#include "Scene.h"
#include "dx12/TopLevelAS.h"
#include "dx12/BottomLevelAS.h"
#include "dx12/CommandQueue.h"
#include "dx12/ResourceUploadBatch.h"
#include "ErrorHandling.h"
#include "StringConversion.h"

#include "../external/d3dx12.h"
#include "../external/stb/stb_image.h"
#include "pbrtParser/Scene.h"

#include <fstream>
#include <algorithm>

#include <wrl/client.h>
using namespace Microsoft::WRL;

static DirectX::XMFLOAT3 PbrtVecToXMFloat(pbrt::vec3f v)
{
    return DirectX::XMFLOAT3{ v.x, v.y, v.z };
}

static DirectX::XMFLOAT2 PbrtVecToXMFloat(pbrt::vec2f v)
{
    return DirectX::XMFLOAT2{ v.x, v.y };
}

static DirectX::XMVECTOR PbrtVecToXMVector(pbrt::vec3f v)
{
    return DirectX::XMLoadFloat3(&PbrtVecToXMFloat(v));
}

static std::string GetDirectory(const std::string& path)
{
    size_t lastSlash = path.find_last_of('/');
    size_t lastBackSlash = path.find_last_of('\\');
    size_t lastDelimiter = std::string::npos;

    if (lastSlash != std::string::npos)
    {
        if (lastBackSlash == std::string::npos)
            lastDelimiter = lastSlash;
        else
            lastDelimiter = std::max(lastSlash, lastBackSlash);
    }
    else if (lastBackSlash != std::string::npos)
        lastDelimiter = lastBackSlash;
    else
        return "";

    return path.substr(0, lastDelimiter);
}

static ComPtr<ID3D12GraphicsCommandList4> CreateTemporaryCommandList(ID3D12Device* device)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ComPtr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    return commandList;
}

static void AddAreaLights(const DirectX::XMFLOAT3* positions, const Scene::Vertex* vertices, uint32_t* indices, uint32_t numTriangles, DirectX::SimpleMath::Vector3 emittedRadiance, std::vector<Scene::AreaLightTriangle>& outAreaLights)
{
    const auto firstNewAreaLightIdx = outAreaLights.size();
    outAreaLights.resize(outAreaLights.size() + numTriangles);

    float totalArea = 0.0f;
    for (uint32_t triangleIdx = 0; triangleIdx < numTriangles; ++triangleIdx)
    {
        Scene::AreaLightTriangle& areaLightTriangle = outAreaLights[firstNewAreaLightIdx + triangleIdx];
        areaLightTriangle.positions[0] = positions[indices[triangleIdx * 3 + 0]];
        areaLightTriangle.positions[1] = positions[indices[triangleIdx * 3 + 1]];
        areaLightTriangle.positions[2] = positions[indices[triangleIdx * 3 + 2]];
        areaLightTriangle.normals[0] = vertices[indices[triangleIdx * 3 + 0]].normal;
        areaLightTriangle.normals[1] = vertices[indices[triangleIdx * 3 + 1]].normal;
        areaLightTriangle.normals[2] = vertices[indices[triangleIdx * 3 + 2]].normal;
        areaLightTriangle.area = (areaLightTriangle.positions[1] - areaLightTriangle.positions[0]).Cross(areaLightTriangle.positions[2] - areaLightTriangle.positions[0]).Length() * 0.5f;
        areaLightTriangle.emittedRadiance = emittedRadiance;
        totalArea += areaLightTriangle.area;
    }
}

static void GenerateNormalsIfMissing(const pbrt::TriangleMesh::SP& triangleShape)
{
    if (!triangleShape->normal.empty())
        return;
        
    triangleShape->normal.resize(triangleShape->vertex.size());
    memset(triangleShape->normal.data(), 0, sizeof(pbrt::vec3f) * triangleShape->normal.size());

    for (auto triangle : triangleShape->index)
    {
        auto v1 = triangleShape->vertex[triangle.x];
        auto v2 = triangleShape->vertex[triangle.y];
        auto v3 = triangleShape->vertex[triangle.z];
        auto triangleNormal = pbrt::math::cross(v2 - v1, v3 - v1);
        triangleShape->normal[triangle.x] = triangleShape->normal[triangle.x] + triangleNormal;
        triangleShape->normal[triangle.y] = triangleShape->normal[triangle.y] + triangleNormal;
        triangleShape->normal[triangle.z] = triangleShape->normal[triangle.z] + triangleNormal;
    }
    for (auto& normal : triangleShape->normal)
        normal = pbrt::math::normalize(normal);
}

static uint32_t LoadPbrtTexture(const std::string& sceneDirectory, const pbrt::Texture::SP& texture, Scene::TextureManager& textures, ResourceUploadBatch& resourceUpload, ID3D12Device* device)
{
    const auto& imageTexture = texture->as<pbrt::ImageTexture>();
    if (!imageTexture)
    {
        LogPrint(LogLevel::Warning, "Texture type '%s' not supported", texture->toString().c_str());
        return textures.GetTextureIndexForColor(DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f), resourceUpload, device);
    }

    return textures.GetTextureIndexForFile(sceneDirectory + "/" + imageTexture->fileName, resourceUpload, device);
}

static Scene::Mesh LoadPbrtMesh(uint32_t index, const pbrt::TriangleMesh::SP& triangleShape, const pbrt::Instance::SP& instance, ID3D12Device5* device, 
                                std::vector<Scene::AreaLightTriangle>& outAreaLights, const std::string& sceneDirectory, Scene::TextureManager& textures, ResourceUploadBatch& resourceUpload)
{
    // Generate normals on the shape, so we can use the binary format of the pbrt library next time.
    GenerateNormalsIfMissing(triangleShape);

    pbrt::DiffuseAreaLightRGB::SP areaLight = triangleShape->areaLight ? triangleShape->areaLight->as<pbrt::DiffuseAreaLightRGB>() : nullptr;

    // todo: Handle material & textures
    //shape->material
    Scene::Mesh mesh;
    mesh.positionBuffer = GraphicsResource::CreateStaticBuffer(Utf8toUtf16(instance->object->name + " Positions").c_str(), sizeof(DirectX::XMFLOAT3) * triangleShape->vertex.size(), device);
    mesh.vertexBuffer = GraphicsResource::CreateStaticBuffer(Utf8toUtf16(instance->object->name + " VB").c_str(), sizeof(Scene::Vertex) * triangleShape->vertex.size(), device);
    mesh.vertexCount = (uint32_t)triangleShape->vertex.size();
    uint64_t indexbufferSize = sizeof(uint32_t) * triangleShape->index.size() * 3;
    mesh.indexBuffer = GraphicsResource::CreateStaticBuffer(Utf8toUtf16(instance->object->name + " IB").c_str(), indexbufferSize, device);
    mesh.indexCount = (uint32_t)triangleShape->index.size() * 3;
    mesh.constantBuffer = GraphicsResource::CreateStaticBuffer(Utf8toUtf16(instance->object->name + " CB").c_str(), sizeof(Scene::MeshConstants), device);
    {
        // Positions
        std::vector<DirectX::XMFLOAT3> positions(triangleShape->vertex.size());
        DirectX::XMFLOAT3* positionBufferUploadData = (DirectX::XMFLOAT3*)resourceUpload.CreateAndMapUploadBuffer(mesh.positionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        for (size_t vertexIdx = 0; vertexIdx < triangleShape->vertex.size(); ++vertexIdx)
            positionBufferUploadData[vertexIdx] = PbrtVecToXMFloat(instance->xfm * triangleShape->vertex[vertexIdx]);

        // Vertices.
        auto normalTransformation = pbrt::math::inverse_transpose(instance->xfm.l);
        std::vector<Scene::Vertex> vertices(triangleShape->vertex.size());
        for (size_t vertexIdx = 0; vertexIdx < triangleShape->vertex.size(); ++vertexIdx)
        {
            auto normal = triangleShape->normal[vertexIdx];
            if (triangleShape->reverseOrientation)
                normal = -normal;
            vertices[vertexIdx].normal = PbrtVecToXMFloat(normalTransformation * normal);
            vertices[vertexIdx].texcoord = DirectX::XMFLOAT2{ 0, 0 };
        }
        if (triangleShape->texcoord.size() == triangleShape->vertex.size())
        {
            for (size_t vertexIdx = 0; vertexIdx < triangleShape->texcoord.size(); ++vertexIdx)
                vertices[vertexIdx].texcoord = DirectX::XMFLOAT2{ triangleShape->texcoord[vertexIdx].x, -triangleShape->texcoord[vertexIdx].y };
        }
        {
            void* vertexBufferUploadData = resourceUpload.CreateAndMapUploadBuffer(mesh.vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            memcpy(vertexBufferUploadData, vertices.data(), sizeof(Scene::Vertex) * triangleShape->vertex.size());
        }

        // Area Lights.
        if (areaLight)
            AddAreaLights(positionBufferUploadData, vertices.data(), (uint32_t*)triangleShape->index.data(), (uint32_t)triangleShape->index.size(), PbrtVecToXMFloat(areaLight->L), outAreaLights);
    }
    {
        void* indexBufferUploadData = resourceUpload.CreateAndMapUploadBuffer(mesh.indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        memcpy(indexBufferUploadData, triangleShape->index.data(), indexbufferSize);
    }
    {
        void* constantBufferUploadData = resourceUpload.CreateAndMapUploadBuffer(mesh.constantBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        auto constants = (Scene::MeshConstants*)constantBufferUploadData;
        constants->MeshIndex = index;

        if (const auto matteMaterial = triangleShape->material->as<pbrt::MatteMaterial>())
        {
            if (matteMaterial->map_kd)
                constants->DiffuseTextureIndex = LoadPbrtTexture(sceneDirectory, matteMaterial->map_kd, textures, resourceUpload, device);
            else
                constants->DiffuseTextureIndex = textures.GetTextureIndexForColor(PbrtVecToXMFloat(matteMaterial->kd), resourceUpload, device);

            if (matteMaterial->sigma != 0.0f || matteMaterial->map_sigma)
                LogPrint(LogLevel::Warning, "Sigma parameter in matte material '%s' not supported", matteMaterial->name.c_str());
        }
        else if (const auto substrateMaterial = triangleShape->material->as<pbrt::SubstrateMaterial>())
        {
            if (substrateMaterial->map_kd)
                constants->DiffuseTextureIndex = LoadPbrtTexture(sceneDirectory, substrateMaterial->map_kd, textures, resourceUpload, device);
            else
                constants->DiffuseTextureIndex = textures.GetTextureIndexForColor(PbrtVecToXMFloat(substrateMaterial->kd), resourceUpload, device);

            if (substrateMaterial->map_ks)
                LogPrint(LogLevel::Warning, "Map KS parameter in substrate material '%s' not supported", substrateMaterial->name.c_str());
            else if (substrateMaterial->ks.x || substrateMaterial->ks.y || substrateMaterial->ks.y)
                LogPrint(LogLevel::Warning, "ks parameter in substrate material '%s' not supported", substrateMaterial->name.c_str());
            if (substrateMaterial->map_bump)
                LogPrint(LogLevel::Warning, "Bump parameter in substrate material '%s' not supported", substrateMaterial->name.c_str());
        }
        else
        {
            constants->DiffuseTextureIndex = textures.GetTextureIndexForColor(DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f), resourceUpload, device);
            LogPrint(LogLevel::Warning, "Material type of material '%s' not supported", triangleShape->material->name.c_str());
        }
        if (areaLight)
        {
            constants->AreaLightRadiance = PbrtVecToXMFloat(areaLight->L);
            constants->IsEmitter = 0xFFFFFFFF;
        }
        else
            constants->IsEmitter = 0;
    }

    return mesh;
}

std::unique_ptr<Scene> Scene::LoadPbrtScene(const std::string& pbrtFilePath, CommandQueue& commandQueue, ID3D12Device5* device)
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
            LogPrint(LogLevel::Failure, "Failed to load scene from pbf: %s", exception.what());
        }
        LogPrint(LogLevel::Success, "Successfully imported pbrt scene from pbf (%s)", pbrtFilePath.c_str());
    }
    else
    {
        try
        {
            pbrtScene = pbrt::importPBRT(pbrtFilePath);
        }
        catch (std::exception& exception)
        {
            LogPrint(LogLevel::Failure, "Failed to load scene from pbrt: %s", exception.what());
        }
        LogPrint(LogLevel::Success, "Successfully imported pbrt scene from pbrt (%s)", pbrtFilePath.c_str());
    }

    if (!pbrtScene)
        return nullptr;
    LogPrint(LogLevel::Info, "Flattening scene...");
    pbrtScene->makeSingleLevel();

    auto scene = std::unique_ptr<Scene>(new Scene());

    LogPrint(LogLevel::Info, "Importing...");

    scene->m_originFilePath = pbrtFilePath;
    if (pbrtScene->film)
    {
        scene->m_screenHeight = (uint32_t)pbrtScene->film->resolution.y;
        scene->m_screenWidth = (uint32_t)pbrtScene->film->resolution.x;
    }

    for (const auto& pbrtCamera : pbrtScene->cameras)
    {
        scene->m_cameras.emplace_back();
        auto& camera = scene->m_cameras.back();
        camera.SetPosition(PbrtVecToXMVector(pbrtCamera->frame.p));
        camera.SetUp(DirectX::XMVector3Normalize(PbrtVecToXMVector(pbrtCamera->frame.l.vy)));
        camera.SetDirection(DirectX::XMVector3Normalize(PbrtVecToXMVector(pbrtCamera->frame.l.vz)));
        camera.SetFovRad(pbrtCamera->fov * ((float)M_PI / 180.0f));
        camera.SnapUpToAxis(); // Makes camera easier to control
    }

    std::string sceneDirectory = GetDirectory(pbrtFilePath);

    auto commandList = CreateTemporaryCommandList(device);
    ResourceUploadBatch uploadBatch(commandList.Get());

    for (const pbrt::Instance::SP& instance : pbrtScene->world->instances)
    {
        for (const pbrt::Shape::SP& shape : instance->object->shapes)
        {
            const auto triangleShape = shape->as<pbrt::TriangleMesh>();
            if (triangleShape)
                scene->m_meshes.push_back(LoadPbrtMesh((uint32_t)scene->m_meshes.size(), triangleShape, instance, device, scene->m_areaLights, sceneDirectory, scene->m_textureManager, uploadBatch));
        }
        for (const pbrt::LightSource::SP& lightSource : instance->object->lightSources)
        {
            // todo.
        }
    }

    if (!pbfFileExists)
    {
        LogPrint(LogLevel::Info, "Saving pbf file...");
        pbrtScene->saveTo(pbfFilePath);
    }


    // todo: be clever about BLAS/TLAS instances

    //scene->m_meshes.resize(1);
    //CreateTestTriangle(scene->m_meshes.back(), device);

    LogPrint(LogLevel::Info, "Creating accelleration datastructure...");
    scene->CreateAccellerationDataStructure(commandList.Get(), device);

    commandList->Close();
    commandQueue.WaitUntilExectionIsFinished(commandQueue.ExecuteCommandList(commandList.Get()));    

    LogPrint(LogLevel::Success, "Successfully loaded scene");
    return scene;
}

void Scene::CreateAccellerationDataStructure(ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device)
{
    std::vector<BottomLevelASMesh> blasMeshes(m_meshes.size());
    for (int i = 0; i < m_meshes.size(); ++i)
    {
        blasMeshes[i].vertexBuffer = { m_meshes[i].positionBuffer->GetGPUVirtualAddress(), sizeof(DirectX::XMFLOAT3) };
        blasMeshes[i].vertexCount = m_meshes[i].vertexCount;
        blasMeshes[i].indexBuffer = m_meshes[i].indexBuffer->GetGPUVirtualAddress();
        blasMeshes[i].indexCount = m_meshes[i].indexCount;
    }
    auto blas = BottomLevelAS::Generate(blasMeshes, commandList, device);

    auto instance = BottomLevelASInstance(blas.get());
    m_tlas = TopLevelAS::Generate({ instance }, commandList, device);
    m_blas.push_back(std::move(blas));
}

const std::string Scene::GetName() const
{
    auto lastSlash = m_originFilePath.find_last_of("/\\");
    auto lastDot = m_originFilePath.find_last_of('.');
    return m_originFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

uint32_t Scene::TextureManager::GetTextureIndexForColor(DirectX::XMFLOAT3 color, ResourceUploadBatch& resourceUpload, ID3D12Device* device)
{
    std::string textureIdentifier = std::to_string(color.x) + std::to_string(color.y) + std::to_string(color.z);
    return GetTextureIndexForColor(textureIdentifier, color, resourceUpload, device);
}

uint32_t Scene::TextureManager::GetTextureIndexForColor(const std::string& textureIdentifier, DirectX::XMFLOAT3 color, ResourceUploadBatch& resourceUpload, ID3D12Device* device)
{
    auto identifierIt = m_textureIdentifierToTextureIndex.find(textureIdentifier);
    if (identifierIt != m_textureIdentifierToTextureIndex.end())
        return identifierIt->second;

    auto texture = TextureResource::CreateTexture2D(Utf8toUtf16(textureIdentifier).c_str(), DXGI_FORMAT_R32G32B32_FLOAT, 1, 1, 1, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, device);
    auto textureData = resourceUpload.CreateAndMapUploadTexture2D(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    *(DirectX::XMFLOAT3*)textureData.pData = color;

    m_textureIdentifierToTextureIndex.insert(std::make_pair(textureIdentifier, (uint32_t)m_textures.size()));
    m_textures.push_back(std::move(texture));
    return (uint32_t)m_textures.size() - 1;
}

uint32_t Scene::TextureManager::GetTextureIndexForFile(const std::string& filename, ResourceUploadBatch& resourceUpload, ID3D12Device* device)
{
    auto identifierIt = m_textureIdentifierToTextureIndex.find(filename);
    if (identifierIt != m_textureIdentifierToTextureIndex.end())
        return identifierIt->second;

    int textureWidth, textureHeight, numComp;
    stbi_uc* loadedImage = stbi_load(filename.c_str(), &textureWidth, &textureHeight, &numComp, 4);
    if (!loadedImage)
    {
        LogPrint(LogLevel::Failure, "Failed to load image from \"%s\": %s", filename.c_str(), stbi_failure_reason());
        GetTextureIndexForColor(filename, DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f), resourceUpload, device);
    }

    // todo: Use stbi_is_hdr to detect hdr formats.
    // todo: Support single channel. (a bit tricky because then we no longer force to 4 channels meaning we need to expand whenever we encounter 3)
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    auto texture = TextureResource::CreateTexture2D(Utf8toUtf16(filename).c_str(), format, (uint32_t)textureWidth, (uint32_t)textureHeight, 1, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, device);
    auto textureData = resourceUpload.CreateAndMapUploadTexture2D(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // Copy row by row since rows can have padding!
    size_t rawRowPitch = (size_t)4 * textureWidth;
    for (int rowIdx = 0; rowIdx < textureHeight; ++rowIdx)
        memcpy((char*)textureData.pData + textureData.RowPitch * rowIdx, loadedImage + rawRowPitch * rowIdx, rawRowPitch);

    stbi_image_free(loadedImage);

    m_textureIdentifierToTextureIndex.insert(std::make_pair(filename.c_str(), (uint32_t)m_textures.size()));
    m_textures.push_back(std::move(texture));
    return (uint32_t)m_textures.size() - 1;
}

Scene::Scene()
{
}

Scene::~Scene()
{
}
