#include "Pch.hpp"

#include "LunarEngine/Engine.hpp"

namespace lunar
{
    void Engine::run()
    {
        init();

        // Main engine loop.
        std::chrono::high_resolution_clock clock{};
        std::chrono::high_resolution_clock::time_point previous_frame{};

        bool quit = false;

        while (!quit)
        {
            const auto current_frame = clock.now();
            const float delta_time = std::chrono::duration<float, std::milli>(current_frame - previous_frame).count();
            previous_frame = current_frame;

            m_event_queue.update();

            while (!m_event_queue.empty())
            {
                const xwin::Event& event = m_event_queue.front();

                if (event.type == xwin::EventType::Close)
                {
                    quit = true;
                }

                m_event_queue.pop();
            }

            update(delta_time);
            render();
        }
    }

    void Engine::init()
    {
        // Get project root directory.
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

        // Create the CrossWindow window and event queue.
        const xwin::WindowDesc window_desc{
            .width = m_window_width,
            .height = m_window_height,
            .centered = true,
            .visible = true,
            .title = "LunarEngine",
            .name = "EngineWindow",
        };

        if (!m_window.create(window_desc, m_event_queue))
        {
            ErrorMessage(L"Failed to create CrossWindow window.");
        }

        // Create the graphics device.
        m_graphics_device = std::make_unique<GraphicsDevice>(m_window_width, m_window_height, m_window.getHwnd());

        // Create triangle index buffer.
        const std::array<uint32_t, 3u> index_buffer_data{0u, 1u, 2u};
        const D3D11_BUFFER_DESC index_buffer_desc{
            .ByteWidth = sizeof(uint32_t) * index_buffer_data.size(),
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = 0u,
            .MiscFlags = 0u,
            .StructureByteStride = 0u,
        };

        m_triangle_index_buffer = m_graphics_device->create_buffer(index_buffer_desc, index_buffer_data.data());

        // Create the triangle vertex buffer.
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

        m_triangle_vertex_buffer = m_graphics_device->create_buffer(vertex_buffer_desc, vertices.data());

        // Create triangle constant buffer.
        const D3D11_BUFFER_DESC constant_buffer_desc{
            .ByteWidth = sizeof(MVPBuffer),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = 0u,
            .MiscFlags = 0u,
            .StructureByteStride = 0u,
        };

        m_triangle_constant_buffer = m_graphics_device->create_buffer(constant_buffer_desc);

        // Create vertex and pixel shaders.
        auto vertex_shader_res =
            m_graphics_device->create_vertex_shader(get_full_asset_path("shaders/HelloTriangle.hlsl"));

        m_triangle_vertex_shader = vertex_shader_res.first;
        auto vertex_shader_blob = vertex_shader_res.second;

        m_triangle_pixel_shader =
            m_graphics_device->create_pixel_shader(get_full_asset_path("shaders/HelloTriangle.hlsl"));

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

        m_input_layout = m_graphics_device->create_input_layout(input_element_descs, vertex_shader_blob.Get());

        // Setup the depth stencil state desc.
        const D3D11_DEPTH_STENCIL_DESC depth_stencil_state_desc{
            .DepthEnable = TRUE,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS,
            .StencilEnable = false,
        };

        m_depth_stencil_state = m_graphics_device->create_depth_stencil_state(depth_stencil_state_desc);

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
        m_rasterizer_state = m_graphics_device->create_rasterizer_state(rasterizer_desc);
    }

    void Engine::update(const float delta_time)
    {
        static float total_time = 0.0f;
        total_time += delta_time * 0.5f;

        const MVPBuffer mvp_buffer_data{
            .model_matrix = Math::XMMatrixScaling(sin(Math::XMConvertToRadians(total_time)), 1.0f, 1.0f),
        };

        m_graphics_device->update_subresources(m_triangle_constant_buffer.Get(), &mvp_buffer_data);
    }

    void Engine::render()
    {
        static const std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
        static const uint32_t offset = 0u;
        static const uint32_t stride = sizeof(Vertex);

        WRL::ComPtr<ID3D11RenderTargetView> render_target_view = m_graphics_device->get_render_target_view();
        WRL::ComPtr<ID3D11DepthStencilView> dsv = m_graphics_device->get_depth_stencil_view();

        auto device_context = m_graphics_device->get_device_context();

        device_context->ClearRenderTargetView(render_target_view.Get(), clear_color.data());
        device_context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 1u);

        device_context->OMSetRenderTargets(1u, render_target_view.GetAddressOf(), dsv.Get());
        device_context->OMSetDepthStencilState(m_depth_stencil_state.Get(), 0u);

        device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        device_context->IASetInputLayout(m_input_layout.Get());
        device_context->IASetIndexBuffer(m_triangle_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0u);
        device_context->IASetVertexBuffers(0u, 1u, m_triangle_vertex_buffer.GetAddressOf(), &stride, &offset);

        device_context->VSSetShader(m_triangle_vertex_shader.Get(), nullptr, 0u);
        device_context->VSSetConstantBuffers(0u, 1u, m_triangle_constant_buffer.GetAddressOf());

        device_context->PSSetShader(m_triangle_pixel_shader.Get(), nullptr, 0u);
        device_context->RSSetViewports(1u, &m_graphics_device->get_viewport());
        device_context->RSSetState(m_rasterizer_state.Get());
        device_context->DrawInstanced(3u, 1u, 0u, 0u);

        m_graphics_device->present();
    }
}