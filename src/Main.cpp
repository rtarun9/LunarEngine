#include <CrossWindow/CrossWindow.h>

#include "Graphics/Device.hpp"

int main()
{
    const uint32_t width = 1080u;
    const uint32_t height = 720u;

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

    // Create the graphics device.
    std::unique_ptr<lunar::gfx::Device> graphics_device =
        std::make_unique<lunar::gfx::Device>(width, height, window.getHwnd());

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

    WRL::ComPtr<ID3D11Buffer> index_buffer = graphics_device->CreateBuffer(index_buffer_desc, index_buffer_data.data());

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

    WRL::ComPtr<ID3D11Buffer> vertex_buffer = graphics_device->CreateBuffer(vertex_buffer_desc, vertices.data());

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

    WRL::ComPtr<ID3D11Buffer> mvp_buffer = graphics_device->CreateBuffer(constant_buffer_desc);

    auto device = graphics_device->GetDevice();
    auto device_context = graphics_device->GetDeviceContext();

    std::pair<WRL::ComPtr<ID3D11VertexShader>, WRL::ComPtr<ID3DBlob>> vertex_shader_res =
        graphics_device->CreateVertexShader("shaders/HelloTriangle.hlsl");
    auto vertex_shader = vertex_shader_res.first;
    auto vertex_shader_blob = vertex_shader_res.second;

    WRL::ComPtr<ID3D11PixelShader> pixel_shader = graphics_device->CreatePixelShader("shaders/HelloTriangle.hlsl");

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

    WRL::ComPtr<ID3D11InputLayout> input_layout =
        graphics_device->CreateInputLayout(input_element_descs, vertex_shader_blob.Get());

    // Setup the depth stencil state desc.
    const D3D11_DEPTH_STENCIL_DESC depth_stencil_state_desc{
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS,
        .StencilEnable = false,
    };

    WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state =
        graphics_device->CreateDepthStencilDesc(depth_stencil_state_desc);

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
    WRL::ComPtr<ID3D11RasterizerState> rasterizer_state = graphics_device->CreateRasterizerDesc(rasterizer_desc);

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

        graphics_device->UpdateSubresources(mvp_buffer.Get(), &mvp_buffer_data);

        // Render.
        static const std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
        static const uint32_t offset = 0u;
        static const uint32_t stride = sizeof(Vertex);

        WRL::ComPtr<ID3D11RenderTargetView> render_target_view = graphics_device->GetRenderTargetView();
        WRL::ComPtr<ID3D11DepthStencilView> dsv = graphics_device->GetDepthStencilView();

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
        device_context->RSSetViewports(1u, &graphics_device->GetViewport());
        device_context->RSSetState(rasterizer_state.Get());
        device_context->DrawInstanced(3u, 1u, 0u, 0u);

        graphics_device->Present();
    }

    return 0;
}