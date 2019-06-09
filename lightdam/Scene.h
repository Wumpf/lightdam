#pragma once

#include "GraphicsResource.h"
#include <memory>
#include <vector>

class Scene
{
public:
    // todo: load from PBR file.
    static std::unique_ptr<Scene> LoadScene(class SwapChain& swapChain, struct ID3D12Device5* device);
    ~Scene();


    struct Mesh
    {
        GraphicsResource vertexBuffer;
        uint32_t vertexCount;
    };

    const std::vector<Mesh>& GetMeshes() { return m_meshes; }

private:

    Scene();

    std::unique_ptr<class TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
};

