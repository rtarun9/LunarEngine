#include "Pch.hpp"

#include "GraphicsDevice.hpp"

namespace lunar::gfx
{
	GraphicsDevice::GraphicsDevice()
	{
		// Create the DXGI factory for enumerating and choosing a adapter.
		constexpr u32 FACTORY_CREATION_FLAGS = [&]()
		{
			if constexpr (LUNAR_DEBUG)
			{
				return DXGI_CREATE_FACTORY_DEBUG;
			}

			return 0;
		}();

		ThrowIfFailed(::CreateDXGIFactory2(FACTORY_CREATION_FLAGS, IID_PPV_ARGS(&m_factory)));

		// Select the adapter with best performance.
		// Adapter represents the display subsystem (i.e the GPU, video memory, etc).
		ThrowIfFailed(m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));
	
		// Display the selected adapter.
		DXGI_ADAPTER_DESC1 adapter_desc{};
		ThrowIfFailed(m_adapter->GetDesc1(&adapter_desc));
		std::wcout << L"Selected adapter : " << adapter_desc.Description << '\n';

		// Enable debug layer in debug builds.
		if constexpr (LUNAR_DEBUG)
		{
			ThrowIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_interface)));

			m_debug_interface->EnableDebugLayer();
			m_debug_interface->SetEnableAutoName(TRUE);
		}

		// Create the D3D12 Device. Represents a virtual adapter and used in creation of most D3D12 resources.
		ThrowIfFailed(::D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
		SetName(m_device.Get(), L"D3D12 Device");

		// Setup info queue for settings breakpoints on incorrect usage of the API.
		if constexpr (LUNAR_DEBUG)
		{
			Microsoft::WRL::ComPtr<ID3D12InfoQueue1> info_queue{};
			ThrowIfFailed(m_device.As(&info_queue));

			ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
		}

		// Create command queues.
		m_direct_command_queue.Init(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Command Queue");
	}
}
