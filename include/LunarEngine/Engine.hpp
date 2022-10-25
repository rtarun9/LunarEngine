#pragma once

#include "GraphicsDevice.hpp"

#include <CrossWindow/CrossWindow.h>

namespace lunar
{
    struct Vertex
    {
        Math::XMFLOAT3 position{};
        Math::XMFLOAT3 color{};
    };

    struct alignas(256) MVPBuffer
    {
        Math::XMMATRIX model_matrix{};
    };

    class Engine
    {
      public:
        Engine() = default;
        ~Engine() { m_graphics_device->get_device_context()->Flush(); }

        void run();

      private:
        void init();
        void update(const float delta_time);
        void render();

        inline std::string get_full_asset_path(const std::string_view asset_path)
        {
            return m_root_directory + asset_path.data();
        }

      private:
        xwin::Window m_window{};
        xwin::EventQueue m_event_queue{};

        std::string m_root_directory{};

        uint32_t m_window_width{1080u};
        uint32_t m_window_height{720u};

        std::unique_ptr<GraphicsDevice> m_graphics_device{};

      private:
        // Rendering 'SandBox' data.
        WRL::ComPtr<ID3D11Buffer> m_triangle_index_buffer{};
        WRL::ComPtr<ID3D11Buffer> m_triangle_vertex_buffer{};

        WRL::ComPtr<ID3D11Buffer> m_triangle_constant_buffer{};
        MVPBuffer m_mvp_buffer_data{};

        WRL::ComPtr<ID3D11VertexShader> m_triangle_vertex_shader{};
        WRL::ComPtr<ID3D11PixelShader> m_triangle_pixel_shader{};

        WRL::ComPtr<ID3D11InputLayout> m_input_layout{};

        WRL::ComPtr<ID3D11RasterizerState> m_rasterizer_state{};
        WRL::ComPtr<ID3D11DepthStencilState> m_depth_stencil_state{};
    };
}