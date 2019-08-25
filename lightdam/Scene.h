#pragma once

#include "dx12/GraphicsResource.h"
#include "../external/SimpleMath.h"
#include "Camera.h"
#include <memory>
#include <vector>
#include <string>

class TopLevelAS;
class CommandQueue;

// A static scene with a DXR Raytracing accelleration structure.
class Scene
{
public:
    // Creates a hardcoded test scene.
    static std::unique_ptr<Scene> LoadTestScene(CommandQueue& commandQueue, struct ID3D12Device5* device);
    // Loads from a PBRT file.
    // Converts to binary format on successful load which will be used automatically if already existing.
    static std::unique_ptr<Scene> LoadPbrtScene(const std::string& pbrtFilePath, CommandQueue& commandQueue, struct ID3D12Device5* device);

    ~Scene();

    // Mesh with uploaded vertex/index buffer.
    struct Mesh
    {
        Mesh() = default;
        Mesh(Mesh&&) = default;

        GraphicsResource vertexBuffer;
        uint32_t vertexCount;
        GraphicsResource indexBuffer;
        uint32_t indexCount;
        GraphicsResource constantBuffer;
    };

    // Exact vertex format used by all vertices in Mesh.
    struct Vertex
    {
        DirectX::SimpleMath::Vector3 position; // todo: unnecessary for hit shader
        DirectX::SimpleMath::Vector3 normal;
        // todo: texcoord missing
    };

    // Exact constant buffer format used by all meshes.
    struct MeshConstants
    {
        uint32_t MeshIndex;
        DirectX::XMFLOAT3 Diffuse;
    };

    struct AreaLightTriangle
    {
        DirectX::SimpleMath::Vector3 positions[3];
        DirectX::SimpleMath::Vector3 normals[3];
        DirectX::SimpleMath::Vector3 emittedRadiance; // The amount of emitted radiance at each point and emitted direction. (this is a 
        float area;
    };

    const std::vector<Mesh>& GetMeshes() const                  { return m_meshes; }
    const std::vector<AreaLightTriangle>& GetAreaLights() const { return m_areaLights; }
    const TopLevelAS& GetTopLevelAccellerationStructure() const { return *m_tlas; }

    // Cameras defined by the scene.
    const std::vector<Camera>& GetCameras() const { return m_cameras; }
    // Retrieves the resolution setting defined by the scene. 0 if no resolution was defined.
    void GetScreenSize(uint32_t& width, uint32_t& height) const { width = m_screenWidth; height = m_screenHeight; }

    // Where the scene originated from.
    const std::string& GetFilePath() const { return m_originFilePath; }
    const std::string GetName() const;

private:

    Scene();

    void CreateAccellerationDataStructure(CommandQueue& commandQueue, ID3D12Device5* device);

    std::string m_originFilePath;

    std::unique_ptr<TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
    std::vector<AreaLightTriangle> m_areaLights;
    std::vector<Camera> m_cameras;
    uint32_t m_screenWidth = 0;
    uint32_t m_screenHeight = 0;
};
