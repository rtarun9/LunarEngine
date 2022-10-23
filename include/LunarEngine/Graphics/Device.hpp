#pragma once

namespace lunar::gfx
{
    class Device
    {
      public:
        Device(const uint32_t window_width, const uint32_t window_height, const HWND window_handle);

        ID3D11Device5* GetDevice() const { return m_device.Get(); }

        ID3D11DeviceContext2* GetDeviceContext() const { return m_device_context.Get(); }

        ID3D11RenderTargetView* GetRenderTargetView() const { return m_render_target_view.Get(); }

        ID3D11DepthStencilView* GetDepthStencilView() const { return m_depth_stencil_view.Get(); }

        IDXGISwapChain1* GetSwapchain() const { return m_swapchain.Get(); }

        D3D11_VIEWPORT& GetViewport() { return m_viewport; }

        void GetRootDirectory();
        void CreateDeviceResources();
        void CreateFrameSizeDependentResources(uint32_t window_width, const uint32_t window_height,
                                               const HWND window_handle);

        void UpdateSubresources(ID3D11Buffer* buffer, const void* data);

        void Present();

        [[nodiscard]] ID3D11Buffer* CreateBuffer(const D3D11_BUFFER_DESC& buffer_desc, const void* dat = nullptr);
        [[nodiscard]] std::pair<ID3D11VertexShader*, ID3DBlob*> CreateVertexShader(const std::string_view shader_path);
        [[nodiscard]] ID3D11PixelShader* CreatePixelShader(const std::string_view shader_path);
        [[nodiscard]] ID3D11InputLayout* CreateInputLayout(
            const std::span<const D3D11_INPUT_ELEMENT_DESC> input_element_descs, ID3DBlob* vertex_shader_blob);
        [[nodiscard]] ID3D11RasterizerState* CreateRasterizerDesc(const D3D11_RASTERIZER_DESC& rasterizer_desc);
        [[nodiscard]] ID3D11DepthStencilState* CreateDepthStencilDesc(
            const D3D11_DEPTH_STENCIL_DESC& depth_stencil_desc);

      private:
        std::string m_root_directory{};

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