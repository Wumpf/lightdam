#pragma once

#include "dx12/GraphicsResource.h"
#include "Camera.h"
#include <memory>
#include <vector>
#include <string>

class TopLevelAS;

// A static scene with a DXR Raytracing accelleration structure.
class Scene
{
public:
    // Creates a hardcoded test scene.
    static std::unique_ptr<Scene> LoadTestScene(class SwapChain& swapChain, struct ID3D12Device5* device);
    // Loads from a PBRT file.
    // Converts to binary format on successful load which will be used automatically if already existing.
    static std::unique_ptr<Scene> LoadPbrtScene(const std::string& pbrtFilePath, class SwapChain& swapChain, struct ID3D12Device5* device);

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
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };

    // Exact constant buffer format used by all meshes.
    struct MeshConstants
    {
        uint32_t MeshIndex;
        DirectX::XMFLOAT3 Diffuse;
    };

    // TODO
    //struct LightSource
    //{
    //    // todo: Support more light sources!

    //    // Direction for directional light source.
    //    DirectX::XMFLOAT4 direction; // xyz vector

    //    // Total radiance emmitted by the light.
    //    DirectX::XMFLOAT4 radiance;  // rgb spectrum
    //};

    const std::vector<Mesh>& GetMeshes() const                  { return m_meshes; }
    const TopLevelAS& GetTopLevelAccellerationStructure() const { return *m_tlas; }

    // Cameras defined by the scene.
    const std::vector<Camera>& GetCameras() const { return m_cameras; }

    // Where the scene originated from.
    const std::string& GetFilePath() const { return m_originFilePath; }
    const std::string GetName() const;

private:

    Scene();

    void CreateAccellerationDataStructure(SwapChain& swapChain, ID3D12Device5* device);

    std::string m_originFilePath;

    std::unique_ptr<TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
    std::vector<Camera> m_cameras;
};

