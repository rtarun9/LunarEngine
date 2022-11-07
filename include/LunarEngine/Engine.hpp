#pragma once

#include "Resources.hpp"

struct SDL_Window;

namespace lunar
{
    class Engine
    {
      public:
        Engine() = default;

        void init();
        void run();

      private:
        void initVulkan();
        void initPipelines();
        void initMeshes();

        void render();

        void cleanup();

      private:
        vk::ShaderModule createShaderModule(const std::string_view shaderPath);

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

        vk::CommandPool m_commandPool{};
        vk::CommandBuffer m_commandBuffer{};

        vk::Semaphore m_presentationSemaphore{};
        vk::Semaphore m_renderSemaphore{};
        vk::Fence m_renderFence{};

        VmaAllocator m_vmaAllocator{};

        vk::PipelineLayout m_trianglePipelineLayout{};
        vk::Pipeline m_trianglePipeline{};

        Mesh m_triangleMesh{};
    };
}