#include "CommandQueue.h"
#include "../ErrorHandling.h"

CommandQueue::CommandQueue(ID3D12Device* device, const wchar_t* name, D3D12_COMMAND_LIST_TYPE type)
    : m_nextFenceSignal(1)
    , m_lastSignaledFenceValue(0)
{
    // Create synchronization objects.
    ThrowIfFailed(device->CreateFence(m_lastSignaledFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Describe and create the graphics queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    m_commandQueue->SetName(name);
}

CommandQueue::~CommandQueue()
{
    m_commandQueue->Signal(m_fence, m_nextFenceSignal);
    WaitUntilExectionIsFinished(m_nextFenceSignal);

    m_commandQueue->Release();
    m_fence->Release();
    CloseHandle(m_fenceEvent);
}

uint64_t CommandQueue::ExecuteCommandLists(unsigned int numCommandLists, ID3D12CommandList* const* commandLists)
{
    m_commandQueue->ExecuteCommandLists(numCommandLists, commandLists);

    ThrowIfFailed(m_commandQueue->Signal(m_fence, m_nextFenceSignal));
    m_lastSignaledFenceValue = m_nextFenceSignal;
    ++m_nextFenceSignal;
    return m_lastSignaledFenceValue;
}

bool CommandQueue::IsExecutionFinished(ExecutionIndex index) const
{
    uint64_t completedValue = m_fence->GetCompletedValue();
    return completedValue >= index;
}

void CommandQueue::WaitUntilExectionIsFinished(ExecutionIndex index)
{
    if (IsExecutionFinished(index))
        return;

    ThrowIfFailed(m_fence->SetEventOnCompletion(index, m_fenceEvent));
    if (WaitForSingleObject(m_fenceEvent, 1000 * 10) == WAIT_TIMEOUT)
        throw std::exception("Waited for more than 10s on gpu work!");
}

void CommandQueue::WaitUntilAllGPUWorkIsFinished()
{
    // Do another signal just in case if anyone executed command lists without going through our api.
    ThrowIfFailed(m_commandQueue->Signal(m_fence, m_nextFenceSignal));
    m_lastSignaledFenceValue = m_nextFenceSignal;
    ++m_nextFenceSignal;

    WaitUntilExectionIsFinished(m_lastSignaledFenceValue);
}