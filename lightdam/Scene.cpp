#include "Scene.h"
#include "TopLevelAS.h"
#include "BottomLevelAS.h"
#include "ErrorHandling.h"

#include <wrl/client.h>
using namespace Microsoft::WRL;

std::unique_ptr<Scene> Scene::LoadScene(ID3D12Device5* device)
{
    auto scene = std::unique_ptr<Scene>(new Scene());

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&commandAllocator)));
    ComPtr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    auto blas = BottomLevelAS::Generate({}, commandList.Get(), device);
    auto instance = BottomLevelASInstance(blas.get());
    scene->m_tlas = TopLevelAS::Generate({ instance }, commandList.Get(), device);
    scene->m_blas.push_back(std::move(blas));

    return scene;
}

Scene::Scene()
{
}

Scene::~Scene()
{
}
