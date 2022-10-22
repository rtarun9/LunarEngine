#include <CrossWindow/CrossWindow.h>

int main()
{
    uint32_t width = 1080u;
    uint32_t height = 720u;

    // Create window using CrossWindow.
    const xwin::WindowDesc window_desc{
        .width = width,
        .height = height,
        .centered = true,
        .visible = true,
        .title = "LunarEngine",
        .name = "EngineWindow",
    };

    xwin::Window window{};
    xwin::EventQueue event_queue{};

    if (!window.create(window_desc, event_queue))
    {
        ErrorMessage(L"Failed to create CrossWindow window.");
    }

    // Create the DXGI factory for enumeration of adapters and creation of swapchain.
    uint32_t factory_creation_flags = 0;
    if constexpr (LUNAR_DEBUG)
    {
        factory_creation_flags = DXGI_CREATE_FACTORY_DEBUG;
    }

    WRL::ComPtr<IDXGIFactory6> factory{};
    ThrowIfFailed(::CreateDXGIFactory2(factory_creation_flags, IID_PPV_ARGS(&factory)));

    // Create the adapter (represents the display subsystem : the GPU, video memory, etc).
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
    const DXGI_SWAP_CHAIN_DESC1 swapchain_desc{
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1u, 0u},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2u,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = 0u,
    };

    ThrowIfFailed(
        factory->CreateSwapChainForHwnd(device.Get(), window.getHwnd(), &swapchain_desc, nullptr, nullptr, &swapchain));

    // Create the render target (texture that will be rendered into).
    WRL::ComPtr<ID3D11RenderTargetView> render_target_view{};
    WRL::ComPtr<ID3D11Texture2D> back_buffer{};

    ThrowIfFailed(swapchain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer)));
    ThrowIfFailed(device->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view));

    // Create depth buffer.
    const D3D11_TEXTURE2D_DESC depth_texture_desc{
        .Width = width,
        .Height = height,
        .MipLevels = 1u,
        .ArraySize = 1u,
        .Format = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = {1u, 0u},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
        .CPUAccessFlags = 0u,
        .MiscFlags = 0u,
    };

    WRL::ComPtr<ID3D11Texture2D> depth_texture{};
    ThrowIfFailed(device->CreateTexture2D(&depth_texture_desc, nullptr, &depth_texture));

    WRL::ComPtr<ID3D11DepthStencilView> dsv{};
    const D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{
        .Format = DXGI_FORMAT_D32_FLOAT,
        .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
        .Flags = 0u,
        .Texture2D{
            .MipSlice = 0u,
        },
    };

    ThrowIfFailed(device->CreateDepthStencilView(depth_texture.Get(), &depth_stencil_view_desc, &dsv));

    // Create the index buffer.
    const std::array<uint32_t, 3u> index_buffer_data{0u, 1u, 2u};
    const D3D11_BUFFER_DESC index_buffer_desc{
        .ByteWidth = sizeof(uint32_t) * index_buffer_data.size(),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = 0u,
        .MiscFlags = 0u,
        .StructureByteStride = 0u,
    };

    const D3D11_SUBRESOURCE_DATA subresource_data{
        .pSysMem = index_buffer_data.data(),
    };

    WRL::ComPtr<ID3D11Buffer> index_buffer{};
    ThrowIfFailed(device->CreateBuffer(&index_buffer_desc, &subresource_data, &index_buffer));

    // Create the vertex buffer.
    struct Vertex
    {
        Math::XMFLOAT3 position{};
        Math::XMFLOAT3 color{};
    };

    const std::array<Vertex, 3> vertices{
        Vertex{Math::XMFLOAT3(-0.5f, -0.5f, 0.0f), Math::XMFLOAT3(1.0f, 0.0f, 0.0f)},
        Vertex{Math::XMFLOAT3(0.0f, 0.5f, 0.0f), Math::XMFLOAT3(0.0f, 1.0f, 0.0f)},
        Vertex{Math::XMFLOAT3(0.5f, -0.5f, 0.0f), Math::XMFLOAT3(0.0f, 0.0f, 1.0f)},
    };

    const D3D11_BUFFER_DESC vertex_buffer_desc{
        .ByteWidth = sizeof(Vertex) * vertices.size(),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0u,
        .MiscFlags = 0u,
        .StructureByteStride = 0u,
    };

    const D3D11_SUBRESOURCE_DATA vertex_subresource_data{
        .pSysMem = vertices.data(),
    };

    WRL::ComPtr<ID3D11Buffer> vertex_buffer{};
    ThrowIfFailed(device->CreateBuffer(&vertex_buffer_desc, &vertex_subresource_data, &vertex_buffer));

    // Create constant buffer.
    struct alignas(256) MVPBuffer
    {
        Math::XMMATRIX model_matrix{};
    };

    const D3D11_BUFFER_DESC constant_buffer_desc{
        .ByteWidth = sizeof(MVPBuffer),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = 0u,
        .MiscFlags = 0u,
        .StructureByteStride = 0u,
    };

    WRL::ComPtr<ID3D11Buffer> mvp_buffer{};
    ThrowIfFailed(device->CreateBuffer(&constant_buffer_desc, nullptr, &mvp_buffer));

    // Get the root project path.
    auto current_directory = std::filesystem::current_path();

    while (!std::filesystem::exists(current_directory / "LunarEngine"))
    {
        if (current_directory.has_parent_path())
        {
            current_directory = current_directory.parent_path();
        }
    }

    const std::string root_dir = (current_directory / "LunarEngine/").string();
    std::cout << "Root Directory: " << root_dir << '\n';

    auto get_full_path = [&](const std::string_view path) { return root_dir + path.data(); };

    // Create the vertex and pixel shaders.
    uint32_t shader_compilation_flag{0u};
    if constexpr (LUNAR_DEBUG)
    {
        shader_compilation_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }

    WRL::ComPtr<ID3D11VertexShader> vertex_shader{};

    WRL::ComPtr<ID3DBlob> vertex_shader_blob{};
    WRL::ComPtr<ID3DBlob> error_blob{};

    const std::string vertex_shader_path = get_full_path("shaders/HelloTriangle.hlsl");

    if (FAILED(::D3DCompileFromFile(StringToWstring(vertex_shader_path).c_str(), nullptr, nullptr, "vs_main", "vs_5_0",
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

    // Create the input layout (i.e tell the GPU how to interpret the Vertex Buffer's).
    const std::array<D3D11_INPUT_ELEMENT_DESC, 2u> input_element_descs{
        D3D11_INPUT_ELEMENT_DESC{
            .SemanticName = "POSITION",
            .SemanticIndex = 0u,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0u,
            .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0u,
        },
        D3D11_INPUT_ELEMENT_DESC{
            .SemanticName = "COLOR",
            .SemanticIndex = 0u,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0u,
            .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0u,
        },
    };

    WRL::ComPtr<ID3D11InputLayout> input_layout{};

    ThrowIfFailed(device->CreateInputLayout(input_element_descs.data(), 2u, vertex_shader_blob->GetBufferPointer(),
                                            vertex_shader_blob->GetBufferSize(), &input_layout));

    // Setup the depth stencil state desc.
    const D3D11_DEPTH_STENCIL_DESC depth_stencil_state_desc{
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS,
        .StencilEnable = false,
    };

    WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state{};
    ThrowIfFailed(device->CreateDepthStencilState(&depth_stencil_state_desc, &depth_stencil_state));

    // Setup rasterizer desc.
    const D3D11_RASTERIZER_DESC rasterizer_desc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_BACK,
        .FrontCounterClockwise = FALSE,
        .DepthBias = 0,
        .DepthBiasClamp = 0.0f,
        .DepthClipEnable = true,
        .MultisampleEnable = false,
    };
    WRL::ComPtr<ID3D11RasterizerState> rasterizer_state{};
    ThrowIfFailed(device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state));

    // Setup viewport.
    const D3D11_VIEWPORT viewport{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = (float)width,
        .Height = (float)height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    // Main engine loop.
    std::chrono::high_resolution_clock clock{};
    std::chrono::high_resolution_clock::time_point previous_frame{};

    bool quit = false;
    while (!quit)
    {
        auto current_frame = clock.now();
        const float delta_time = std::chrono::duration<float, std::milli>(previous_frame - current_frame).count();
        previous_frame = current_frame;

        static float total_time{0.0f};
        total_time += delta_time * 0.05f;

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
        const MVPBuffer mvp_buffer_data{
            .model_matrix = Math::XMMatrixScaling(sin(Math::XMConvertToRadians(total_time)), 1.0f, 1.0f),
        };

        device_context->UpdateSubresource(mvp_buffer.Get(), 0u, nullptr, &mvp_buffer_data, 0u, 0u);

        // Render.
        static const std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
        static const uint32_t offset = 0u;
        static const uint32_t stride = sizeof(Vertex);

        device_context->ClearRenderTargetView(render_target_view.Get(), clear_color.data());
        device_context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 1u);

        device_context->OMSetRenderTargets(1u, render_target_view.GetAddressOf(), dsv.Get());
        device_context->OMSetDepthStencilState(depth_stencil_state.Get(), 0u);

        device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        device_context->IASetInputLayout(input_layout.Get());
        device_context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0u);
        device_context->IASetVertexBuffers(0u, 1u, vertex_buffer.GetAddressOf(), &stride, &offset);

        device_context->VSSetShader(vertex_shader.Get(), nullptr, 0u);
        device_context->VSSetConstantBuffers(0u, 1u, mvp_buffer.GetAddressOf());

        device_context->PSSetShader(pixel_shader.Get(), nullptr, 0u);
        device_context->RSSetViewports(1u, &viewport);
        device_context->RSSetState(rasterizer_state.Get());
        device_context->DrawInstanced(3u, 1u, 0u, 0u);

        ThrowIfFailed(swapchain->Present(1u, 0u));
    }

    return 0;
}