#pragma once

#include "Resources.hpp"
#include "Types.hpp"

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
        void initSwapchain();
        void initCommandObjects();
        void initSyncPrimitives();
        void initDescriptors();
        void initPipelines();
        void initMeshes();
        void initScene();

        void render();

        void cleanup();

        FrameData& getCurrentFrameData() { return m_frameData[m_frameNumber % FRAME_COUNT]; }

      private:
        [[nodiscard]] vk::ShaderModule createShaderModule(const std::string_view shaderPath);

        // Creates GPU buffer and updates the deletion queue internally.
        // If data is a nullptr, it will create a buffer with CPU write access.
        [[nodiscard]] Buffer createGPUBuffer(const vk::BufferCreateInfo bufferCreateInfo, const void* data = nullptr);

        // Copies the data from staging buffer to the respective src buffers. Called upload rather than 'copy buffers' as it sounds close to uploading data onto GPU only buffer.
        // Might find a more suitable name as two different names (i.e copy buffer / upload buffer) is being used in the project now.
        void uploadBuffers();

        [[nodiscard]] vk::Pipeline createPipeline(const PipelineCreationDesc& pipelineCreationDesc,
                                                  const vk::PipelineLayout& pipelineLayout);

        // Mesh creation functions.
        [[nodiscard]] Mesh createMesh(const std::string_view modelPath);

      public:
        static constexpr uint32_t FRAME_COUNT = 2u;

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

        vk::CommandPool m_transferCommandPool{};
        vk::CommandBuffer m_transferCommandBuffer{};

        std::array<FrameData, FRAME_COUNT> m_frameData{};

        VmaAllocator m_vmaAllocator{};

        Image m_depthImage{};
        vk::ImageView m_depthImageView{};
        vk::Format m_depthImageFormat{vk::Format::eD32Sfloat};
        
        // Holds all the staging buffers and the GPU only buffers together. All staging buffers will be destroyed at the end of the delete function.
        // This means we can batch commands rather than submitted a buffer to the queue for the creation of each buffer. 
        std::vector<BufferUploadData> m_bufferUploadData{};
        DeletionQueue m_uploadBufferDeletionQueue{};

        // Each frame will have a descriptor set, but the layout and pool they are allocated from remain unique.
        vk::DescriptorPool m_descriptorPool{};
        vk::DescriptorSetLayout m_globalDescriptorSetLayout{};

        // Scene management : Materials (i.e pipeline + pipeline layout) and meshes will be unordered maps, and render objects will be a material + mesh + transform buffer.
        std::unordered_map<std::string, Mesh> m_meshes{};
        std::unordered_map<std::string, Material> m_materials{};

        std::vector<RenderObject> m_renderObjects{};
    };
}