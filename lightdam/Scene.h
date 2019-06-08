#pragma once

#include <memory>
#include <vector>

class Scene
{
public:
    // todo: load from PBR file.
    static std::unique_ptr<Scene> LoadScene(struct ID3D12Device5* device);
    ~Scene();

private:

    Scene();

    std::unique_ptr<class TopLevelAS> m_tlas;
    std::vector<std::unique_ptr<class BottomLevelAS>> m_blas;
};

