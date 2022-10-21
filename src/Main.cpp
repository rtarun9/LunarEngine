#include <CrossWindow/CrossWindow.h>

int main()
{
    uint32_t width = 1080u;
    uint32_t height = 720u;

    // Create window using CrossWindow.
    xwin::WindowDesc window_desc{};
    window_desc.title = "LunarEngine";
    window_desc.name = "EngineWindow";
    window_desc.visible = true;
    window_desc.width = width;
    window_desc.height = height;
    window_desc.centered = true;

    xwin::Window window{};
    xwin::EventQueue event_queue{};

    if (!window.create(window_desc, event_queue))
    {
        return -1;
    }

    // Create the DXGI factory for enumeration of adapters and creation of swapchain.
    uint32_t factory_creation_flags = 0;
    if constexpr (LUNAR_DEBUG)
    {
        factory_creation_flags = DXGI_CREATE_FACTORY_DEBUG;
    }

    WRL::ComPtr<IDXGIFactory6> factory{};
    ThrowIfFailed(::CreateDXGIFactory2(factory_creation_flags, IID_PPV_ARGS(&factory)));

    // Create the adapter (represents the display subsystem : the GPU, video memory, etc)..
    WRL::ComPtr<IDXGIAdapter1> adapter{};
    ThrowIfFailed(
        factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
    DXGI_ADAPTER_DESC adapter_desc{};
    ThrowIfFailed(adapter->GetDesc(&adapter_desc));
    std::wcout << L"Chosen adapter: " << adapter_desc.Description << L'\n';

    // Create the D3D11 Device, used for creation of all resources.
    // The DeviceContext is used for execution of GPU commands, setting the pipeline state, etc.
    WRL::ComPtr<ID3D11Device5> device{};
    WRL::ComPtr<ID3D11DeviceContext2> device_context{};

    const D3D_FEATURE_LEVEL required_feature_level = D3D_FEATURE_LEVEL_11_0;

    // D3D11CreateDevice only accepts a ID3D11Device/Context, while we want a ID3D11Device5 and Context2.
    // So, creating a base type first and then using the As method provided by ComPtr<> to convert them to the newer
    // versions.
    {
        uint32_t device_creation_flags = 0u;
        if constexpr (LUNAR_DEBUG)
        {
            device_creation_flags = D3D11_CREATE_DEVICE_DEBUG;
        }

        WRL::ComPtr<ID3D11Device> device_base{};
        WRL::ComPtr<ID3D11DeviceContext> device_context_base{};

        ThrowIfFailed(::D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, device_creation_flags,
                                          &required_feature_level, 1u, D3D11_SDK_VERSION, &device_base, nullptr,
                                          &device_context_base));

        device_base.As(&device);
        device_context_base.As(&device_context);
    }

    // Enable the debug layer in debug builds. Also enable breakpoints on incorrect API usage.
    WRL::ComPtr<ID3D11Debug> debug_controller{};
    WRL::ComPtr<ID3D11InfoQueue> info_queue{};

    if constexpr (LUNAR_DEBUG)
    {
        ThrowIfFailed(device.As(&debug_controller));

        ThrowIfFailed(device.As(&info_queue));
        ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
        ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE));
    }

    // Create the frame resources.
    // Swapchain handles swapping of back buffers and stores resources we render to and presents them to the display.
    WRL::ComPtr<IDXGISwapChain1> swapchain{};
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.Width = width;
    swapchain_desc.Height = height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc = {1u, 0u};
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = 2u;
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapchain_desc.Flags = 0u;

    ThrowIfFailed(
        factory->CreateSwapChainForHwnd(device.Get(), window.getHwnd(), &swapchain_desc, nullptr, nullptr, &swapchain));

    // Create the render target (texture that will be rendered into).
    WRL::ComPtr<ID3D11RenderTargetView> render_target_view{};
    WRL::ComPtr<ID3D11Texture2D> back_buffer{};

    ThrowIfFailed(swapchain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer)));
    ThrowIfFailed(device->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view));

    // Create the index buffer.
    const std::array<uint32_t, 3u> index_buffer_data{0u, 1u, 2u};
    D3D11_BUFFER_DESC index_buffer_desc{};
    index_buffer_desc.ByteWidth = sizeof(uint32_t) * index_buffer_data.size();
    index_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = 0u;
    index_buffer_desc.MiscFlags = 0u;
    index_buffer_desc.StructureByteStride = 0u;

    D3D11_SUBRESOURCE_DATA subresource_data{};
    subresource_data.pSysMem = index_buffer_data.data();

    WRL::ComPtr<ID3D11Buffer> index_buffer{};
    ThrowIfFailed(device->CreateBuffer(&index_buffer_desc, &subresource_data, &index_buffer));

    // Get the root project path.
    auto current_directory = std::filesystem::current_path();

    while (!std::filesystem::exists(current_directory  / "LunarEngine"))
    {
        if (current_directory.has_parent_path())
        {
            current_directory = current_directory.parent_path();
        }
    }

    const std::string root_dir = (current_directory / "LunarEngine/").string();
    std::cout << "Root Directory: " << root_dir << '\n';

    // Create the vertex and pixel shaders.
    uint32_t shader_compilation_flag{0u};
    if constexpr (LUNAR_DEBUG)
    {
        shader_compilation_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }

    WRL::ComPtr<ID3D11VertexShader> vertex_shader{};

    WRL::ComPtr<ID3DBlob> vertex_shader_blob{};
    WRL::ComPtr<ID3DBlob> error_blob{};

    const std::string vertex_shader_path = root_dir + std::string("shaders/HelloTriangle.hlsl");

    if (FAILED(::D3DCompileFromFile(StringToWstring(vertex_shader_path).c_str(), nullptr, nullptr,
                                    "vs_main", "vs_5_0",
                                    shader_compilation_flag, 0u, &vertex_shader_blob, &error_blob)))
    {
        const char* error_message = (const char*)error_blob->GetBufferPointer();
        std::cout << error_message;
    }

    ThrowIfFailed(device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(),
                                             vertex_shader_blob->GetBufferSize(), nullptr, &vertex_shader));
    
    WRL::ComPtr<ID3D11PixelShader> pixel_shader{};

    WRL::ComPtr<ID3DBlob> pixel_shader_blob{};

    const std::string pixel_shader_path = root_dir + std::string("shaders/HelloTriangle.hlsl");

    if (FAILED(::D3DCompileFromFile(StringToWstring(pixel_shader_path).c_str(), nullptr, nullptr, "ps_main", "ps_5_0",
                                    shader_compilation_flag, 0u, &pixel_shader_blob, &error_blob)))
    {
        const char* error_message = (const char*)error_blob->GetBufferPointer();
        std::cout << error_message;
    }

    ThrowIfFailed(device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(),
                                            nullptr, &pixel_shader));

    // Main engine loop.
    bool quit = false;
    while (!quit)
    {
        event_queue.update();

        while (!event_queue.empty())
        {
            const xwin::Event& event = event_queue.front();

            if (event.type == xwin::EventType::Close)
            {
                quit = true;
            }

            event_queue.pop();
        }

        // Update.

        // Render.
    }

    return 0;
}