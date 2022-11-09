#pragma once

#include "Resources.hpp"

struct SDL_Window;

namespace tinygltf
{
    struct Model;
}

namespace lunar
{
    class Engine
    {
      public:
        Engine() = default;
        ~Engine();

        void init();
        void run();

      private:
        void initVulkan();
        void initPipelines();
        void initMeshes();

        void render();

        void cleanup();

      private:
        [[nodiscard]] vk::ShaderModule createShaderModule(const std::string_view shaderPath);

        // Creates GPU buffer.
        [[nodiscard]] Buffer createGPUBuffer(const vk::BufferCreateInfo bufferCreateInfo, const void* data);

        // Mesh creation functions.
        [[nodiscard]] Mesh createMesh(const std::string_view modelPath);

      public:
        static constexpr uint32_t FRAME_COUNT = 3u;

      private:
        SDL_Window* m_window{};
        vk::Extent2D m_windowExtent{};
        uint64_t m_frameNumber{};

        std::string m_rootDirectory{};
        DeletionQueue m_deletionQueue{};

        // Core vulkan structures.
        vk::Instance m_instance{};
        vk::DebugUtilsMessengerEXT m_debugMessenger{};
        vk::SurfaceKHR m_surface{};
        vk::PhysicalDevice m_physicalDevice{};
        vk::Device m_device{};

        vk::SwapchainKHR m_swapchain{};
        uint32_t m_swapchainImageCount{};
        vk::Format m_swapchainImageFormat{};
        std::vector<vk::Image> m_swapchainImages{};
        std::vector<vk::ImageView> m_swapchainImageViews{};

        vk::Queue m_graphicsQueue{};
        uint32_t m_graphicsQueueIndex{};

        vk::Queue m_transferQueue{};
        uint32_t m_transferQueueIndex{};

        vk::CommandPool m_commandPool{};
        vk::CommandBuffer m_commandBuffer{};

        vk::CommandPool m_transferCommandPool{};
        vk::CommandBuffer m_transferCommandBuffer{};

        vk::Semaphore m_presentationSemaphore{};
        vk::Semaphore m_renderSemaphore{};
        vk::Fence m_renderFence{};

        VmaAllocator m_vmaAllocator{};

        Image m_depthTexture{};
        vk::ImageView m_depthTextureView{};
        vk::Format m_depthTextureFormat{vk::Format::eD32Sfloat};

        vk::PipelineLayout m_pipelineLayout{};
        vk::Pipeline m_pipeline{};

        Mesh m_triangleMesh{};
        Mesh m_suzanneMesh{};
    };
}