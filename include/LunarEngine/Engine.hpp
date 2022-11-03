#pragma once

struct SDL_Window;

namespace lunar
{
    class Engine
    {
      public:
        void init();
        void run();

      private:
        void createDeviceResources();
        void createWindowDependentResources(const Uint2 dimensions);

      public:
        static constexpr uint32_t FRAME_COUNT = 3u;

      private:
        SDL_Window* m_window{};
        HWND m_windowHandle{};

        Uint2 m_dimensions{};

        WRL::ComPtr<IDXGIFactory6> m_factory{};
        WRL::ComPtr<ID3D12Debug5> m_debugController{};

        WRL::ComPtr<IDXGIAdapter2> m_adapter{};

        WRL::ComPtr<ID3D12Device5> m_device{};
        WRL::ComPtr<ID3D12DebugDevice1> m_debugDevice{};
        WRL::ComPtr<ID3D12InfoQueue> m_infoQueue{};

        WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue{};
        WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator{};

        uint32_t m_frameIndex{};
        HANDLE m_fenceEvent{};
        WRL::ComPtr<ID3D12Fence> m_fence{};
        uint64_t m_fenceValue{};

        WRL::ComPtr<IDXGISwapChain3> m_swapchain{};
        D3D12_VIEWPORT m_viewport{};
        D3D12_RECT m_scissorRect{};

        WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap{};
        uint32_t m_rtvDescriptorHeapSize{};
        std::array<WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_renderTargetViews{};
    };
}