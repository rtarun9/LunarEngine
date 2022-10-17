#pragma once

namespace lunar::gfx
{
	class CommandQueue
	{
	public:
		void Init(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE command_list_type, const std::wstring_view name);

		// Synchronization functions.
		void WaitForGPU(const u32 frame_index);
		void Flush();

		void WaitForFenceValue(const u64 fence_value);

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_command_queue{};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_command_list{};

		struct CommandFrame
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator{};
			u64 fence_value{};
		};

		std::array<CommandFrame, NUMBER_OF_FRAMES> m_command_frame{};

		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence{};
		HANDLE m_fence_event{};
	};
}


