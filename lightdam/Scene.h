#pragma once

#include "dx12/GraphicsResource.h"
#include "../external/SimpleMath.h"
#include "Camera.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

class TopLevelAS;
class CommandQueue;
class ResourceUploadBatch;

// A static scene with a DXR Raytracing accelleration structure.
class Scene
{
public:
    // Loads from a PBRT file.
    // Converts to binary format on successful load which will be used automatically if already existing.
    static std::unique_ptr<Scene> LoadPbrtScene(const std::string& pbrtFilePath, CommandQueue& commandQueue, struct ID3D12Device5* device);

    ~Scene();

    // Mesh with uploaded vertex/index buffer.
    struct Mesh
    {
        Mesh() = default;
        Mesh(Mesh&&) = default;

        GraphicsResource positionBuffer;
        GraphicsResource vertexBuffer;
        uint32_t vertexCount;
        GraphicsResource indexBuffer;
        uint32_t indexCount;
        GraphicsResource constantBuffer;
    };

    // Vertex format used by all vertices in Mesh. (GPU layout)
    struct Vertex
    {
        // Position not needed since everything we need is in the accelleration datastructure!
        DirectX::SimpleMath::Vector3 normal;
        DirectX::SimpleMath::Vector2 texcoord;
    };

    // Constant buffer format used by all meshes. (GPU layout)
    struct MeshConstants
    {
        uint32_t MeshIndex;
        uint32_t IsMetal;
        uint32_t IsEmitter;
        uint32_t DiffuseTextureIndex;
        DirectX::XMFLOAT3 AreaLightRadiance;
        float _padding0;
        DirectX::XMFLOAT3 Eta;
        float Roughness;
        DirectX::XMFLOAT3 Ks;
        float _padding1;
    };

    // Info struct on an area light (CPU only!)
    struct AreaLightTriangle
    {
        DirectX::SimpleMath::Vector3 positions[3];
        DirectX::SimpleMath::Vector3 normals[3];
        DirectX::SimpleMath::Vector3 emittedRadiance; // The amount of emitted radiance at each point and emitted direction.
        float area;
    };

    const std::vector<Mesh>& GetMeshes() const                  { return m_meshes; }
    const std::vector<TextureResource>& GetTextures() const     { return m_textureManager.m_textures; }
    const std::vector<AreaLightTriangle>& GetAreaLights() const { return m_areaLights; }
    const TopLevelAS& GetTopLevelAccellerationStructure() const { return *m_tlas; }

    // Cameras defined by the scene.
    const std::vector<Camera>& GetCameras() const { return m_cameras; }
    // Retrieves the resolution setting defined by the scene. 0 if no resolution was defined.
    void GetScreenSize(uint32_t& width, uint32_t& height) const { width = m_screenWidth; height = m_screenHeight; }

    // Where the scene originated from.
    const std::string& GetFilePath() const { return m_originFilePath; }
    const std::string GetName() const;

    class TextureManager
    {
    public:
        uint32_t GetTextureIndexForColor(DirectX::XMFLOAT3 color, ResourceUploadBatch& resourceUpload, ID3D12Device* device);
        uint32_t GetTextureIndexForFile(const std::string& filename, ResourceUploadBatch& resourceUpload, ID3D12Device* device);

        std::vector<TextureResource> m_textures;
        std::unordered_map<std::string, uint32_t> m_textureIdentifierToTextureIndex;

    private:
        uint32_t GetTextureIndexForColor(const std::string& textureIdentifier, DirectX::XMFLOAT3 color, ResourceUploadBatch& resourceUpload, ID3D12Device* device);
    };

private:

    Scene();

    void CreateAccellerationDataStructure(ID3D12GraphicsCommandList4* commandList, ID3D12Device5* device);

    std::string m_originFilePath;

    std::unique_ptr<TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
    std::vector<AreaLightTriangle> m_areaLights;
    std::vector<Camera> m_cameras;
    uint32_t m_screenWidth = 0;
    uint32_t m_screenHeight = 0;

    TextureManager m_textureManager;
};
