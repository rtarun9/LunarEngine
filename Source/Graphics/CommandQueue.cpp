#include "Pch.hpp"

#include "CommandQueue.hpp"

namespace lunar::gfx
{
	void CommandQueue::Init(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE command_list_type, const std::wstring_view name)
	{
		// Command queue is the execution port for the GPU.
		// Provides methods for submitting command lists, synchronization, etc.
		const D3D12_COMMAND_QUEUE_DESC command_queue_desc
		{
			.Type = command_list_type,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0u
		};

		ThrowIfFailed(device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&m_command_queue)));
		SetName(m_command_queue.Get(), name);

		// Create command frames.
		for (const u32 index : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			const std::wstring command_allocator_name = name.data() + std::wstring(L" Allocator ") + std::to_wstring(index);
			ThrowIfFailed(device->CreateCommandAllocator(command_list_type, IID_PPV_ARGS(&m_command_frame[index].command_allocator)));
		}

		// Create command list.
		ThrowIfFailed(device->CreateCommandList(0u, command_list_type, m_command_frame[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_command_list)));
		const std::wstring command_list_name = name.data() + std::wstring(L" Command List");

		SetName(m_command_list.Get(), command_list_name);
		ThrowIfFailed(m_command_list->Close());

		// Create synchronization primitives.
		ThrowIfFailed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		const std::wstring fence_name = name.data() + std::wstring(L" Fence");
		SetName(m_fence.Get(), fence_name);

		m_fence_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fence_event)
		{
			FatalError(L"Failed to create fence event.");
		}
	}

	void CommandQueue::WaitForGPU(const u32 frame_index)
	{
		ThrowIfFailed(m_command_queue->Signal(m_fence.Get(), m_command_frame[frame_index].fence_value));

		ThrowIfFailed(m_fence->SetEventOnCompletion(m_command_frame[frame_index].fence_value, m_fence_event));
		WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE);

		m_command_frame[frame_index].fence_value++;
	}

	void CommandQueue::Flush()
	{
		for (const u32 index : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			WaitForGPU(index);
		}
	}

	void CommandQueue::WaitForFenceValue(const u64 fence_value)
	{
		const u64 completed_fence_value = m_fence->GetCompletedValue();
		if (completed_fence_value < fence_value)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(fence_value, m_fence_event));
			WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE);
		}
	}
}