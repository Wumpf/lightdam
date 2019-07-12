#pragma once

#include "dx12/GraphicsResource.h"
#include "Camera.h"
#include <memory>
#include <vector>
#include <string>

class TopLevelAS;

// A static scene with DXR Raytracing accelleration structure.
class Scene
{
public:
    // todo: load from PBR file.
    static std::unique_ptr<Scene> LoadTestScene(class SwapChain& swapChain, struct ID3D12Device5* device);
    static std::unique_ptr<Scene> LoadPbrtScene(const std::string& pbrtFilePath, class SwapChain& swapChain, struct ID3D12Device5* device);
    ~Scene();

    struct Mesh
    {
        Mesh() = default;
        Mesh(Mesh&&) = default;

        GraphicsResource vertexBuffer;
        uint32_t vertexCount;
        GraphicsResource indexBuffer;
        uint32_t indexCount;
    };

    const std::vector<Mesh>& GetMeshes() const                  { return m_meshes; }
    const TopLevelAS& GetTopLevelAccellerationStructure() const { return *m_tlas; }

    const std::vector<Camera>& GetCameras() const { return m_cameras; }

    const std::string& GetSceneFilePath() const { return m_originFilePath; }

private:

    Scene();

    void CreateAccellerationDataStructure(SwapChain& swapChain, ID3D12Device5* device);

    std::string m_originFilePath;

    std::unique_ptr<TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
    std::vector<Camera> m_cameras;
};

