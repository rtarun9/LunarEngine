#pragma once

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
        void render();

        void cleanup();

      public:
        static constexpr uint32_t FRAME_COUNT = 3u;

      private:
        SDL_Window* m_window{};
        vk::Extent2D m_windowExtent{};
        uint64_t m_frameNumber{};

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

        vk::RenderPass m_renderPass{};
        std::vector<vk::Framebuffer> m_framebuffers{};

        vk::Semaphore m_presentationSemaphore{};
        vk::Semaphore m_renderSemaphore{};
        vk::Fence m_renderFence{};
    };
}