#pragma once

#include "dx12/GraphicsResource.h"
#include <memory>
#include <vector>

class TopLevelAS;

// A static scene.
// Created from a pbrt file, loaded into gpu memory.
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
        GraphicsResource indexBuffer;
        uint32_t indexCount;
    };

    const std::vector<Mesh>& GetMeshes() const                  { return m_meshes; }
    const TopLevelAS& GetTopLevelAccellerationStructure() const { return *m_tlas; }

private:

    Scene();

    std::unique_ptr<TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;

    std::vector<Mesh> m_meshes;
};

