#pragma once

namespace lunar
{
    class GraphicsDevice
    {
      public:
        GraphicsDevice(const uint32_t window_width, const uint32_t window_height, const HWND window_handle);

        // Public getters.
        ID3D11Device5* get_device() const { return m_device.Get(); }

        ID3D11DeviceContext2* get_device_context() const { return m_device_context.Get(); }

        ID3D11RenderTargetView* get_render_target_view() const { return m_render_target_view.Get(); }

        ID3D11DepthStencilView* get_depth_stencil_view() const { return m_depth_stencil_view.Get(); }

        IDXGISwapChain1* get_swapchain() const { return m_swapchain.Get(); }

        D3D11_VIEWPORT& get_viewport() { return m_viewport; }

        void create_device_resources();
        void create_window_size_dependent_resources(uint32_t window_width, const uint32_t window_height,
                                                    const HWND window_handle);

        void update_subresources(ID3D11Buffer* buffer, const void* data);

        void present();
        
        // Creation functions.
        [[nodiscard]] WRL::ComPtr<ID3D11Buffer> create_buffer(const D3D11_BUFFER_DESC& buffer_desc, const void* dat = nullptr);
        [[nodiscard]] std::pair<WRL::ComPtr<ID3D11VertexShader>, WRL::ComPtr<ID3DBlob>> create_vertex_shader(const std::string_view shader_path);
        [[nodiscard]] WRL::ComPtr<ID3D11PixelShader> create_pixel_shader(const std::string_view shader_path);
        [[nodiscard]] WRL::ComPtr<ID3D11InputLayout> create_input_layout(
            const std::span<const D3D11_INPUT_ELEMENT_DESC> input_element_descs, ID3DBlob* vertex_shader_blob);
        [[nodiscard]] WRL::ComPtr<ID3D11RasterizerState> create_rasterizer_state(const D3D11_RASTERIZER_DESC& rasterizer_desc);
        [[nodiscard]] WRL::ComPtr<ID3D11DepthStencilState> create_depth_stencil_state(
            const D3D11_DEPTH_STENCIL_DESC& depth_stencil_desc);

      private:
        WRL::ComPtr<IDXGIFactory6> m_factory{};
        WRL::ComPtr<IDXGIAdapter1> m_adapter{};

        WRL::ComPtr<ID3D11Device5> m_device{};
        WRL::ComPtr<ID3D11DeviceContext2> m_device_context{};

        WRL::ComPtr<ID3D11Debug> m_debug_controller{};
        WRL::ComPtr<ID3D11InfoQueue> m_info_queue{};

        WRL::ComPtr<IDXGISwapChain1> m_swapchain{};

        WRL::ComPtr<ID3D11RenderTargetView> m_render_target_view{};

        WRL::ComPtr<ID3D11Texture2D> m_depth_stencil_texture{};
        WRL::ComPtr<ID3D11DepthStencilView> m_depth_stencil_view{};

        D3D11_VIEWPORT m_viewport{};
    };
}