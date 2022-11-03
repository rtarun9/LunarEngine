#include "Engine.hpp"

#include <SDL.h>
#include <SDL_syswm.h>

namespace lunar
{
    void Engine::init()
    {
        createDeviceResources();
        createWindowDependentResources(m_dimensions);
    }

    void Engine::run()
    {
        // Set DPI awareness.
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Initialize SDL2 and create window.
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        {
            fatalError("Failed to initialize SDL2.");
        }

        // Get monitor dimensions.
        SDL_DisplayMode displayMode{};
        if (SDL_GetCurrentDisplayMode(0, &displayMode) < 0)
        {
            fatalError("Failed to get display mode");
        }

        const uint32_t monitorWidth = displayMode.w;
        const uint32_t monitorHeight = displayMode.h;

        // Window must cover 85% of the screen.
        m_dimensions = {
            .x = static_cast<uint32_t>(monitorWidth * 0.85f),
            .y = static_cast<uint32_t>(monitorHeight * 0.85f),
        };

        m_window = SDL_CreateWindow("LunarEngine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_dimensions.x,
                                    m_dimensions.y, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS);
        if (!m_window)
        {
            fatalError("Failed to create SDL2 window.");
        }

        // Get the window handle.
        SDL_SysWMinfo wmInfo{};
        SDL_GetWindowWMInfo(m_window, &wmInfo);
        m_windowHandle = wmInfo.info.win.window;

        init();

        // Main run loop.
        bool quit{false};
        SDL_Event event{};

        while (!quit)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }

                const uint8_t* keyboardState = SDL_GetKeyboardState(nullptr);
                if (keyboardState[SDL_SCANCODE_ESCAPE])
                {
                    quit = true;
                }
            }
        }
    }

    void Engine::createDeviceResources()
    {
        uint32_t dxgiFactoryFlags = 0u;

        // Set factory flag and get debug interface in debug builds.
        if constexpr (LUNAR_DEBUG)
        {
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

            throwIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)));
            m_debugController->EnableDebugLayer();
            m_debugController->SetEnableAutoName(TRUE);
            m_debugController->SetEnableGPUBasedValidation(TRUE);
        }

        throwIfFailed(::CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

        // Create adapter.
        throwIfFailed(
            m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

        DXGI_ADAPTER_DESC2 adapterDesc{};
        throwIfFailed(m_adapter->GetDesc2(&adapterDesc));
        std::wcout << "Chosen Adapter : " << adapterDesc.Description << '\n';

        throwIfFailed(::D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

        // In debug build get debug device and setup info queue.
        if constexpr (LUNAR_DEBUG)
        {
            throwIfFailed(m_device.As(&m_debugDevice));

            throwIfFailed(m_device.As(&m_infoQueue));
            throwIfFailed(m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            throwIfFailed(m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
            throwIfFailed(m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
        }

        // Create command queues.
        const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc{
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&m_directCommandQueue)));

        // Create command allocator.
        throwIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

        // Create fence.
        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        // Create descriptor heaps.
        const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = FRAME_COUNT,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
    }

    void Engine::createWindowDependentResources(const Uint2 dimensions)
    {
        // Create swapchain if not created.
        if (!m_swapchain)
        {
            const DXGI_SWAP_CHAIN_DESC1 swapChainDesc{
                .Width = dimensions.x,
                .Height = dimensions.y,
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .Stereo = FALSE,
                .SampleDesc = {1u, 0u},
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = FRAME_COUNT,
                .Scaling = DXGI_SCALING_NONE,
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
                .Flags = 0u,
            };

            WRL::ComPtr<IDXGISwapChain1> swapchain{};
            throwIfFailed(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_windowHandle, &swapChainDesc,
                                                            nullptr, nullptr, &swapchain));
            throwIfFailed(swapchain.As(&m_swapchain));
        }
        else
        {
            // Resize swapchain if function is called when swapchain is already created.
            throwIfFailed(
                m_swapchain->ResizeBuffers(FRAME_COUNT, dimensions.x, dimensions.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0u));
        }

        m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

        m_scissorRect = {
            .left = 0l, .top = 0l, .right = static_cast<long>(dimensions.x), .bottom = static_cast<long>(dimensions.y)};

        m_viewport = {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(dimensions.x),
            .Height = static_cast<float>(dimensions.y),
            .MinDepth = 0.0f,
            .MaxDepth = 0.0f,
        };

        // Create RTV's.
        m_rtvDescriptorHeapSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (const uint32_t i : std::views::iota(0u, FRAME_COUNT))
        {
            WRL::ComPtr<ID3D12Resource> backBuffer{};
            throwIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

            m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorHeapSize;
        }
    }
}