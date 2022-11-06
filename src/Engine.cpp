#include "Engine.hpp"

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

namespace lunar
{
    void Engine::init()
    {
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

        // Get root directory.
        auto currentDirectory = std::filesystem::current_path();

        // Keep going up one path until you hit the src folder, implying that the currentDirectory will by the project
        // root directory.
        while (!std::filesystem::exists(currentDirectory / "src"))
        {
            if (currentDirectory.has_parent_path())
            {
                currentDirectory = currentDirectory.parent_path();
            }
            else
            {
                fatalError("Root Directory not found!");
            }
        }

        m_rootDirectory = currentDirectory.string() + "/";

        // Initialize vulkan.
        initVulkan();

        // Setup the pipelines.
        initPipelines();
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
                                           .require_api_version(1, 3, 0)
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

        // Specify that we require Vulkan 1.3's dynamic rendering feature.
        const vk::PhysicalDeviceVulkan13Features features{
            .synchronization2 = true,
            .dynamicRendering = true,
        };

        // Get the physical adapter that can render to the surface.
        vkb::PhysicalDeviceSelector vkbPhysicalDeviceSelector{vkbInstance};
        const auto vkbPhysicalDevice = vkbPhysicalDeviceSelector.set_minimum_version(1, 3)
                                           .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                           .allow_any_gpu_device_type(false)
                                           .set_required_features_13(features)
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
        // also, the commands recorded must be compatible with the graphics queue.
        const vk::CommandPoolCreateInfo commandPoolCreateInfo = {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = m_graphicsQueueIndex,
        };

        m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

        // Create the command buffer.

        // Specify it is primarily, implying it can be sent for execution on a queue. Secondary buffers act as
        // subcommands to a primary buffer.
        const vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1u,
        };

        m_commandBuffer = m_device.allocateCommandBuffers(commandBufferAllocateInfo).at(0);

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

    void Engine::initPipelines()
    {
        // Create shader modules.
        const vk::ShaderModule triangleVertexShaderModule = createShaderModule("shaders/TriangleVS.cso");
        const vk::ShaderModule trianglePixelShaderModule = createShaderModule("shaders/TrianglePS.cso");

        // Create pipeline shader stages.
        const vk::PipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = triangleVertexShaderModule,
            .pName = "VsMain",
        };

        const vk::PipelineShaderStageCreateInfo pixelShaderStageCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = trianglePixelShaderModule,
            .pName = "PsMain",
        };

        // Setup input state.
        const vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};

        // Setup primitive topology.
        const vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false,
        };

        // Set rasterization state.
        const vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
            .depthClampEnable = false,
            .rasterizerDiscardEnable = false,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = false,
        };

        // Setup default multisample state.
        const vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};

        // Setup default color blend attachment state.
        const vk::PipelineColorBlendAttachmentState colorBlendAttachmentState = {
            .blendEnable = false,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        const vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
            .logicOpEnable = false,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1u,
            .pAttachments = &colorBlendAttachmentState,
        };

        // Setup viewport and scissor state.
        const vk::Viewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_windowExtent.width),
            .height = static_cast<float>(m_windowExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        const vk::Rect2D scissor = {
            .offset = {0, 0},
            .extent = m_windowExtent,
        };

        const vk::PipelineViewportStateCreateInfo viewportStateCreateInfo = {
            .viewportCount = 1u,
            .pViewports = &viewport,
            .scissorCount = 1u,
            .pScissors = &scissor,
        };

        // Create empty pipeline layout.
        m_trianglePipelineLayout = m_device.createPipelineLayout({});

        // Create the pipeline.
        const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
            vertexShaderStageCreateInfo,
            pixelShaderStageCreateInfo,
        };

        const vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            .colorAttachmentCount = 1u,
            .pColorAttachmentFormats = &m_swapchainImageFormat,
        };

        const vk::GraphicsPipelineCreateInfo triangleGraphicsPIpelineStateCreateInfo = {
            .pNext = &pipelineRenderingCreateInfo,
            .stageCount = 2u,
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputStateCreateInfo,
            .pInputAssemblyState = &inputAssemblyStateCreateInfo,
            .pViewportState = &viewportStateCreateInfo,
            .pRasterizationState = &rasterizationStateCreateInfo,
            .pMultisampleState = &multisampleStateCreateInfo,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlendStateCreateInfo,
            .pDynamicState = nullptr,
            .layout = m_trianglePipelineLayout,
        };

        const auto trianglePipelineResult =
            m_device.createGraphicsPipeline(nullptr, triangleGraphicsPIpelineStateCreateInfo);

        vkCheck(trianglePipelineResult.result);

        m_trianglePipeline = trianglePipelineResult.value;
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

        // Causing some errors, not entirely sure why.
        // SDL_DestroyWindow(m_window);
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

        // Start rendering.

        // Transition image into writable format before rendering.
        const vk::ImageSubresourceRange subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1u,
            .baseArrayLayer = 0u,
            .layerCount = 1u,
        };

        const vk::ImageMemoryBarrier imageToAttachmentBarrier = {
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = m_swapchainImages[swapchainImageIndex],
            .subresourceRange = subresourceRange,
        };

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eNone,
                            vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::DependencyFlagBits::eByRegion,
                            0u,
                            nullptr,
                            0u,
                            nullptr,
                            1u,
                            &imageToAttachmentBarrier);

        const vk::RenderingAttachmentInfo colorAttachmentInfo = {
            .imageView = m_swapchainImageViews[swapchainImageIndex],
            .imageLayout = vk::ImageLayout::eAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearValue,
        };

        const vk::RenderingInfo renderingInfo = {
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = m_windowExtent,
                },
            .layerCount = 1u,
            .viewMask = 0u,
            .colorAttachmentCount = 1u,
            .pColorAttachments = &colorAttachmentInfo,
        };

        cmd.beginRendering(renderingInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_trianglePipeline);

        cmd.draw(3u, 1u, 0u, 0u);

        cmd.endRendering();

        // Transition image to presentable format.
        const vk::ImageMemoryBarrier attachmentToPresentationBarrier = {
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .image = m_swapchainImages[swapchainImageIndex],
            .subresourceRange = subresourceRange,
        };

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits::eNone,
                            vk::DependencyFlagBits::eByRegion,
                            0u,
                            nullptr,
                            0u,
                            nullptr,
                            1u,
                            &attachmentToPresentationBarrier);

        cmd.end();

        // Prepare to submit the command buffer.
        const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        // Presentation semaphore is ready when the swapchain image is ready.
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

        // Wait for the render semaphore to be signaled (will happen after commands submitted to the queue is
        // completed).
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

        m_device.destroyFence(m_renderFence);
        m_device.destroySemaphore(m_renderSemaphore);
        m_device.destroySemaphore(m_presentationSemaphore);

        m_device.destroyCommandPool(m_commandPool);

        m_device.destroySwapchainKHR(m_swapchain);

        for (const uint32_t i : std::views::iota(0u, m_swapchainImageCount))
        {
            m_device.destroyImageView(m_swapchainImageViews[i]);
        }

        m_device.destroy();

        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_instance.destroySurfaceKHR(m_surface);
        m_instance.destroy();
    }

    vk::ShaderModule Engine::createShaderModule(const std::string_view shaderPath)
    {
        const std::string fullShaderPath = m_rootDirectory + shaderPath.data();

        // Data is in binary format, and place the file pointer to the end so retrieving size is easy.
        std::ifstream shaderBytecodeFile{fullShaderPath, std::ios::ate | std::ios::binary};
        if (!shaderBytecodeFile.is_open())
        {
            fatalError(std::string("Failed to read shader file : ") + fullShaderPath);
        }

        const size_t fileSize = static_cast<size_t>(shaderBytecodeFile.tellg());

        // Spirv expects the buffer to be on a uint32_t. So, resize the buffer accordingly.
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        // Place file pointer to the beginning.
        shaderBytecodeFile.seekg(0);

        shaderBytecodeFile.read((char*)buffer.data(), fileSize);

        shaderBytecodeFile.close();

        // Create the shader module. Size must be in bytes.
        const vk::ShaderModuleCreateInfo shaderModuleCreateInfo = {
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data(),
        };

        const vk::ShaderModule shaderModule = m_device.createShaderModule(shaderModuleCreateInfo);
        return shaderModule;
    }
}
