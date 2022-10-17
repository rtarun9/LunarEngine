#pragma once

#include "CommandQueue.hpp"

namespace lunar::gfx
{
	class GraphicsDevice
	{
	public:
		GraphicsDevice();

		CommandQueue* const GetDirectCommandQueue() { return &m_direct_command_queue; }

	private:
		Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory{};
		Microsoft::WRL::ComPtr<IDXGIAdapter2> m_adapter{};

		Microsoft::WRL::ComPtr<ID3D12Debug5> m_debug_interface{};
		
		Microsoft::WRL::ComPtr<ID3D12Device5> m_device{};

		CommandQueue m_direct_command_queue{};
	};
}
