#include "Engine.hpp"

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

namespace lunar
{
    void Engine::init()
    {
        // Set DPI awareness.
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Initialize SDL2 and create window.
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            fatalError("Failed to initialize SDL2.");
        }

        // Get monitor dimensions.
        SDL_DisplayMode displayMode{};
        if (SDL_GetCurrentDisplayMode(0, &displayMode) < 0)
        {
            fatalError("Failed to get display mode");
        }

        const uint32_t monitorWidth = displayMode.w;
        const uint32_t monitorHeight = displayMode.h;

        // Window must cover 85% of the screen.
        m_windowExtent = vk::Extent2D{
            .width = static_cast<uint32_t>(monitorWidth * 0.85f),
            .height = static_cast<uint32_t>(monitorHeight * 0.85f),
        };

        m_window = SDL_CreateWindow("LunarEngine",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    m_windowExtent.width,
                                    m_windowExtent.height,
                                    SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN);
        if (!m_window)
        {
            fatalError("Failed to create SDL2 window.");
        }

        // Initialize vulkan.
        initVulkan();
    }

    void Engine::initVulkan()
    {
        // Get the instance and enable validation layers.
        // Creating a instance initializes the Vulkan library and lets the application tell information about itself
        // (only applicable if a AAA Game Engine and such).

        constexpr bool enableDebugLayer = LUNAR_DEBUG == true ? true : false;

        vkb::InstanceBuilder instanceBuilder{};
        const auto vkbInstanceResult = instanceBuilder.set_app_name("Lunar Engine")
                                           .request_validation_layers(enableDebugLayer)
                                           .use_default_debug_messenger()
                                           .require_api_version(1, 1, 0)
                                           .build();
        if (!vkbInstanceResult)
        {
            fatalError("Failed to create vulkan instance.");
        }

        const vkb::Instance vkbInstance = vkbInstanceResult.value();

        m_instance = vkbInstance.instance;

        // Get the debug messenger.
        m_debugMessenger = vkbInstance.debug_messenger;

        // Get the surface of the window opened by SDL (i.e get the underlying native platform surface).
        VkSurfaceKHR surface{};
        SDL_Vulkan_CreateSurface(m_window, m_instance, &surface);
        m_surface = surface;

        // Get the physical adapter that can render to the surface.
        vkb::PhysicalDeviceSelector vkbPhysicalDeviceSelector{vkbInstance};
        const auto vkbPhysicalDevice = vkbPhysicalDeviceSelector.set_minimum_version(1, 1)
                                           .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                           .allow_any_gpu_device_type(false)
                                           .set_surface(surface)
                                           .select()
                                           .value();

        m_physicalDevice = vkbPhysicalDevice;

        // Display the physical device chosen.
        std::cout << "Physical Device Chosen : " << vkbPhysicalDevice.name << '\n';

        // Create the Vulkan Device (i.e the logical device, used for creation of Buffers, Textures, etc).
        const vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
        const vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

        m_device = vkbDevice.device;

        // Initialize the swapchain and get the swapchain images and image views.
        // Swapchain provides ability to store and render the rendering results to a surface.
        vkb::SwapchainBuilder vkbSwapchainBuilder{vkbPhysicalDevice, vkbDevice, m_surface};
        vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder.use_default_format_selection()
                                          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
                                          .set_desired_extent(m_windowExtent.width, m_windowExtent.height)
                                          .build()
                                          .value();

        m_swapchain = vkbSwapchain.swapchain;
        m_swapchainImageCount = vkbSwapchain.image_count;

        m_swapchainImages.reserve(m_swapchainImageCount);
        const auto vkbSwapchainImages = vkbSwapchain.get_images().value();
        for (const auto swapchainImage : vkbSwapchainImages)
        {
            m_swapchainImages.emplace_back(swapchainImage);
        }

        m_swapchainImageViews.reserve(m_swapchainImageCount);
        const auto vkbSwapchainImageViews = vkbSwapchain.get_image_views().value();
        for (const auto swapchainImageView : vkbSwapchainImageViews)
        {
            m_swapchainImageViews.emplace_back(swapchainImageView);
        }

        m_swapchainImageFormat = vk::Format(vkbSwapchain.image_format);

        // Get the command queue and family (i.e type of queue).
        m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_graphicsQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        // Create the command pool.

        // Specify that we much be able to reset individual command buffers created from this pool.
        // also, the commands recorded must be compatable with the graphics queue.
        const vk::CommandPoolCreateInfo commandPoolCreateInfo = {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = m_graphicsQueueIndex,
        };

        m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

        // Create the command buffer.

        // Specify it is primariy, implying it can be sent for execution on a queue. Secondary buffers act as
        // subcommands to a primary buffer.
        const vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1u,
        };

        m_commandBuffer = m_device.allocateCommandBuffers(commandBufferAllocateInfo).at(0);

        // Create the renderpass.
        // Basically handles resource (primarily) image transitions. Framebuffers are created for a specific renderpass.

        // Specify the color attachment information that will be used by the renderpass.
        // When the color attachment is loaded, clear it and store when renderpass ends.
        // The initial layout is not of concern, but after renderpass ends it should be ready for presentation.
        const vk::AttachmentDescription colorAttachmentDescription = {
            .format = m_swapchainImageFormat,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
        };

        // Specify the subpass. The index of attachment will be used to index into the pInputAttachments field in the
        // subpass description.
        const vk::AttachmentReference colorAttachmentReference = {
            .attachment = 0u,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
        };

        const vk::SubpassDescription subpassDescription = {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1u,
            .pColorAttachments = &colorAttachmentReference,
        };

        const vk::RenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 1u,
            .pAttachments = &colorAttachmentDescription,
            .subpassCount = 1u,
            .pSubpasses = &subpassDescription,
        };

        m_renderPass = m_device.createRenderPass(renderPassCreateInfo);

        // Create the framebuffer (basically acts as link between attachment of renderpass and the actual images that
        // are rendered to).
        vk::FramebufferCreateInfo framebufferCreateInfo{
            .renderPass = m_renderPass,
            .attachmentCount = 1u,
            .width = m_windowExtent.width,
            .height = m_windowExtent.height,
            .layers = 1u,
        };

        m_framebuffers.resize(m_swapchainImageCount);
        for (const uint32_t i : std::views::iota(0u, m_swapchainImageCount))
        {
            framebufferCreateInfo.pAttachments = &m_swapchainImageViews[i];
            m_framebuffers[i] = m_device.createFramebuffer(framebufferCreateInfo);
        }

        // Create synchronization primitives.
        // Specify that the Signaled flag is set while creating the fence.
        // Base case for the first loop (CPU - GPU sync).
        const vk::FenceCreateInfo fenceCreateInfo = {.flags = vk::FenceCreateFlagBits::eSignaled};

        m_renderFence = m_device.createFence(fenceCreateInfo);

        // Create semaphores (GPU - GPU sync).
        const vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
        m_renderSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
        m_presentationSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
    }

    void Engine::run()
    {
        // Initialize SDL and the graphics backend.
        init();

        // Main run loop.
        bool quit{false};
        SDL_Event event{};

        while (!quit)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }

                const uint8_t* keyboardState = SDL_GetKeyboardState(nullptr);
                if (keyboardState[SDL_SCANCODE_ESCAPE])
                {
                    quit = true;
                }
            }

            render();

            m_frameNumber++;
        }

        // Cleanup.

        cleanup();

        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    void Engine::render()
    {
        // Wait for the GPU to finish execution of the last frame. Max timeout is 1 second.
        vkCheck(m_device.waitForFences(1u, &m_renderFence, true, ONE_SECOND_IN_NANOSECOND));

        // Reset fence.
        vkCheck(m_device.resetFences(1u, &m_renderFence));

        // Request image from swapchain.
        // Signal the presentation semaphore when image is acquired.
        uint32_t swapchainImageIndex{};
        vkCheck(m_device.acquireNextImageKHR(m_swapchain,
                                             ONE_SECOND_IN_NANOSECOND,
                                             m_presentationSemaphore,
                                             {},
                                             &swapchainImageIndex));

        m_commandBuffer.reset();

        vk::CommandBuffer& cmd = m_commandBuffer;

        vk::CommandBufferBeginInfo commandBufferBeginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

        cmd.begin(commandBufferBeginInfo);

        const vk::ClearValue clearValue = {
            .color = {std::array{0.0f, 0.0f, abs(sin(m_frameNumber / 120.0f)), 1.0f}},
        };

        // Start main renderpass.
        const vk::RenderPassBeginInfo renderPassBeginInfo = {
            .renderPass = m_renderPass,
            .framebuffer = m_framebuffers[swapchainImageIndex],
            .renderArea =
                {
                    .offset = {0u, 0u},
                    .extent = m_windowExtent,
                },
            .clearValueCount = 1u,
            .pClearValues = &clearValue,
        };

        cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        cmd.endRenderPass();

        cmd.end();

        // Prepare to submit the command buffer.
        const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        const vk::SubmitInfo submitInfo = {
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = &m_presentationSemaphore,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1u,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1u,
            .pSignalSemaphores = &m_renderSemaphore,
        };

        vkCheck(m_graphicsQueue.submit(1u, &submitInfo, m_renderFence));

        // Setup for presentation.
        const vk::PresentInfoKHR presentInfo = {
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = &m_renderSemaphore,
            .swapchainCount = 1u,
            .pSwapchains = &m_swapchain,
            .pImageIndices = &swapchainImageIndex,
        };

        vkCheck(m_graphicsQueue.presentKHR(presentInfo));
    }

    void Engine::cleanup()
    {
        // Cleanup is done in the reverse order of creation.

        m_device.waitIdle();

        m_device.destroyRenderPass(m_renderPass);

        m_device.destroyCommandPool(m_commandPool);

        m_device.destroySwapchainKHR(m_swapchain);

        for (const uint32_t i : std::views::iota(0u, m_swapchainImageCount))
        {
            m_device.destroyFramebuffer(m_framebuffers[i]);
            m_device.destroyImageView(m_swapchainImageViews[i]);
        }

        m_device.destroy();

        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_instance.destroySurfaceKHR(m_surface);
        m_instance.destroy();
    }
}