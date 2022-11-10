#pragma once

namespace lunar
{
    struct Buffer
    {
        vk::Buffer buffer{};
        VmaAllocation allocation{};
    };

    struct Image
    {
        vk::Image image{};
        VmaAllocation allocation{};
    };

    struct FrameData
    {
        vk::Fence renderFence{};
        vk::Semaphore renderSemaphore{};
        vk::Semaphore presentationSemaphore{};

        vk::CommandPool graphicsCommandPool{};
        vk::CommandBuffer graphicsCommandBuffer{};

        Buffer sceneBuffer{};
        vk::DescriptorSet globalDescriptorSet{};
    };
}