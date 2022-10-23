#include "Pch.hpp"

#include "Graphics/Device.hpp"

namespace lunar::gfx
{
    Device::Device(const uint32_t window_width, const uint32_t window_height, const HWND window_handle)
    {
        GetRootDirectory();
        CreateDeviceResources();
        CreateFrameSizeDependentResources(window_width, window_height, window_handle);
    }

    void Device::GetRootDirectory()
    {
        std::filesystem::path current_directory = std::filesystem::current_path();

        while (!std::filesystem::exists(current_directory / "LunarEngine"))
        {
            if (current_directory.has_parent_path())
            {
                current_directory = current_directory.parent_path();
            }
        }

        m_root_directory = (current_directory / "LunarEngine/").string();
        std::cout << "Root Directory: " << m_root_directory << '\n';
    }

    void Device::CreateDeviceResources()
    {
        // Create the DXGI factory for enumeration of adapters and creation of swapchain.
        uint32_t factory_creation_flags = 0;
        if constexpr (LUNAR_DEBUG)
        {
            factory_creation_flags = DXGI_CREATE_FACTORY_DEBUG;
        }

        ThrowIfFailed(::CreateDXGIFactory2(factory_creation_flags, IID_PPV_ARGS(&m_factory)));

        // Create the adapter (represents the display subsystem : the GPU, video memory, etc).
        ThrowIfFailed(
            m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

        DXGI_ADAPTER_DESC adapter_desc{};
        ThrowIfFailed(m_adapter->GetDesc(&adapter_desc));
        std::wcout << L"Chosen adapter: " << adapter_desc.Description << L'\n';

        // Create the D3D11 Device, used for creation of all resources.
        // The DeviceContext is used for execution of GPU commands, setting the pipeline state, etc.

        const D3D_FEATURE_LEVEL required_feature_level = D3D_FEATURE_LEVEL_11_0;

        // D3D11CreateDevice only accepts a ID3D11Device/Context, while we want a ID3D11Device5 and Context2.
        // So, creating a base type first and then using the As method provided by ComPtr<> to convert them to the newer
        // versions.

        uint32_t device_creation_flags = 0u;
        if constexpr (LUNAR_DEBUG)
        {
            device_creation_flags = D3D11_CREATE_DEVICE_DEBUG;
        }

        WRL::ComPtr<ID3D11Device> device_base{};
        WRL::ComPtr<ID3D11DeviceContext> device_context_base{};

        ThrowIfFailed(::D3D11CreateDevice(m_adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, device_creation_flags,
                                          &required_feature_level, 1u, D3D11_SDK_VERSION, &device_base, nullptr,
                                          &device_context_base));

        device_base.As(&m_device);
        device_context_base.As(&m_device_context);

        // Enable the debug layer in debug builds. Also enable breakpoints on incorrect API usage.
        if constexpr (LUNAR_DEBUG)
        {
            ThrowIfFailed(m_device.As(&m_debug_controller));

            ThrowIfFailed(m_device.As(&m_info_queue));
            ThrowIfFailed(m_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            ThrowIfFailed(m_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
            ThrowIfFailed(m_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE));
        }
    }

    void Device::CreateFrameSizeDependentResources(uint32_t window_width, const uint32_t window_height,
                                                   const HWND window_handle)
    {
        // Create the frame resources.
        // Swapchain handles swapping of back buffers and stores resources we render to and presents them to the
        // display.
        const DXGI_SWAP_CHAIN_DESC1 swapchain_desc{
            .Width = window_width,
            .Height = window_height,
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

        ThrowIfFailed(m_factory->CreateSwapChainForHwnd(m_device.Get(), window_handle, &swapchain_desc, nullptr,
                                                        nullptr, &m_swapchain));

        // Create the render target (texture that will be rendered into).
        WRL::ComPtr<ID3D11Texture2D> back_buffer{};

        ThrowIfFailed(m_swapchain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer)));
        ThrowIfFailed(m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, &m_render_target_view));

        // Create depth buffer.
        const D3D11_TEXTURE2D_DESC depth_texture_desc{
            .Width = window_width,
            .Height = window_height,
            .MipLevels = 1u,
            .ArraySize = 1u,
            .Format = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {1u, 0u},
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_DEPTH_STENCIL,
            .CPUAccessFlags = 0u,
            .MiscFlags = 0u,
        };

        ThrowIfFailed(m_device->CreateTexture2D(&depth_texture_desc, nullptr, &m_depth_stencil_texture));

        const D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
            .Flags = 0u,
            .Texture2D{
                .MipSlice = 0u,
            },
        };

        ThrowIfFailed(m_device->CreateDepthStencilView(m_depth_stencil_texture.Get(), &depth_stencil_view_desc,
                                                       &m_depth_stencil_view));

        // Setup viewport.
        m_viewport = {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(window_width),
            .Height = static_cast<float>(window_height),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };
    }

    void Device::UpdateSubresources(ID3D11Buffer* buffer, const void* data)
    {
        m_device_context->UpdateSubresource(buffer, 0u, nullptr, data, 0u, 0u);
    }

    void Device::Present() { ThrowIfFailed(m_swapchain->Present(1u, 0u)); }

    ID3D11Buffer* Device::CreateBuffer(const D3D11_BUFFER_DESC& buffer_desc, const void* data)
    {
        ID3D11Buffer* buffer{nullptr};

        D3D11_SUBRESOURCE_DATA subresource_data{};
        if (data)
        {
            subresource_data = {
                .pSysMem = data,
            };
        }

        ThrowIfFailed(m_device->CreateBuffer(&buffer_desc, data ? &subresource_data : nullptr, &buffer));

        return buffer;
    }

    std::pair<ID3D11VertexShader*, ID3DBlob*> Device::CreateVertexShader(const std::string_view shader_path)
    {
        // Create the vertex shader (compiling using D3DCompileFromFile).
        uint32_t shader_compilation_flag{0u};
        if constexpr (LUNAR_DEBUG)
        {
            shader_compilation_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        }
        else
        {
            shader_compilation_flag = D3DCOMPILE_OPTIMIZATION_LEVEL3;
        }

        ID3D11VertexShader* vertex_shader{nullptr};

        ID3DBlob* vertex_shader_blob{};
        WRL::ComPtr<ID3DBlob> error_blob{};

        const std::string vertex_shader_path = m_root_directory + shader_path.data();

        if (FAILED(::D3DCompileFromFile(StringToWstring(vertex_shader_path).c_str(), nullptr, nullptr, "vs_main",
                                        "vs_5_0", shader_compilation_flag, 0u, &vertex_shader_blob, &error_blob)))
        {
            const char* error_message = (const char*)error_blob->GetBufferPointer();
            std::cout << error_message;
        }

        ThrowIfFailed(m_device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(),
                                                   vertex_shader_blob->GetBufferSize(), nullptr, &vertex_shader));

        return {vertex_shader, vertex_shader_blob};
    }

    ID3D11PixelShader* Device::CreatePixelShader(const std::string_view shader_path)
    {
        uint32_t shader_compilation_flag{0u};
        if constexpr (LUNAR_DEBUG)
        {
            shader_compilation_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        }
        else
        {
            shader_compilation_flag = D3DCOMPILE_OPTIMIZATION_LEVEL3;
        }

        ID3D11PixelShader* pixel_shader{nullptr};

        WRL::ComPtr<ID3DBlob> pixel_shader_blob{};
        WRL::ComPtr<ID3DBlob> error_blob{};

        const std::string pixel_shader_path = m_root_directory + shader_path.data();

        if (FAILED(::D3DCompileFromFile(StringToWstring(pixel_shader_path).c_str(), nullptr, nullptr, "ps_main",
                                        "ps_5_0", shader_compilation_flag, 0u, &pixel_shader_blob, &error_blob)))
        {
            const char* error_message = (const char*)error_blob->GetBufferPointer();
            std::cout << error_message;
        }

        ThrowIfFailed(m_device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(),
                                                  pixel_shader_blob->GetBufferSize(), nullptr, &pixel_shader));

        return pixel_shader;
    }

    ID3D11InputLayout* Device::CreateInputLayout(const std::span<const D3D11_INPUT_ELEMENT_DESC> input_element_descs,
                                                 ID3DBlob* vertex_shader_blob)
    {
        ID3D11InputLayout* input_layout{nullptr};

        ThrowIfFailed(m_device->CreateInputLayout(
            input_element_descs.data(), static_cast<uint32_t>(input_element_descs.size()),
            vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &input_layout));

        return input_layout;
    }

    ID3D11RasterizerState* Device::CreateRasterizerDesc(const D3D11_RASTERIZER_DESC& rasterizer_desc)
    {
        ID3D11RasterizerState* rasterizer_state{nullptr};

        ThrowIfFailed(m_device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state));

        return rasterizer_state;
    }

    ID3D11DepthStencilState* Device::CreateDepthStencilDesc(const D3D11_DEPTH_STENCIL_DESC& depth_stencil_desc)
    {
        ID3D11DepthStencilState* depth_stencil_state{nullptr};

        ThrowIfFailed(m_device->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state));

        return depth_stencil_state;
    }
}