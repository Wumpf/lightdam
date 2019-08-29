#pragma once

#include <d3d12.h>
#include <stdint.h>

// A command queue with a submission fence/counter
class CommandQueue
{
public:
    CommandQueue(ID3D12Device* device, const wchar_t* name, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
    ~CommandQueue();

    using ExecutionIndex = uint64_t;

    // Executes command lists and bumps the execute counter.
    // Returns the previous execution index - if this index is marked "done", this command is finished. 
    ExecutionIndex ExecuteCommandList(ID3D12CommandList* commandLists) { return ExecuteCommandLists(1, &commandLists); }
    ExecutionIndex ExecuteCommandLists(unsigned int numCommandLists, ID3D12CommandList* const* commandLists);

    bool IsExecutionFinished(ExecutionIndex index) const;
    void WaitUntilExectionIsFinished(ExecutionIndex index);
    void WaitUntilAllGPUWorkIsFinished();

    ExecutionIndex GetNextExecutionIndex() const { return m_nextFenceSignal; }
    ExecutionIndex GetLastExecutionIndex() const { return m_lastSignaledFenceValue; }

    ID3D12CommandQueue* Get() const         { return m_commandQueue; }
    ID3D12CommandQueue* operator ->() const { return m_commandQueue; }

private:
    ID3D12CommandQueue* m_commandQueue;
    ID3D12Fence* m_fence;

    void* m_fenceEvent;
    ExecutionIndex m_nextFenceSignal;
    ExecutionIndex m_lastSignaledFenceValue;
};
