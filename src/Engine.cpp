#include "Engine.hpp"

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace lunar
{
    Engine::~Engine()
    {
        // Cleanup.
        cleanup();

        // Causing some errors, not entirely sure why.
        // SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

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

        // Initialize all meshes.
        initMeshes();
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

        m_deletionQueue.pushFunction([=]() { vkDestroyInstance(m_instance, nullptr); });

        // Get the debug messenger.
        m_debugMessenger = vkbInstance.debug_messenger;

        m_deletionQueue.pushFunction([=]()
                                     { vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger, nullptr); });

        // Get the surface of the window opened by SDL (i.e get the underlying native platform surface).
        VkSurfaceKHR surface{};
        SDL_Vulkan_CreateSurface(m_window, m_instance, &surface);
        m_surface = surface;

        m_deletionQueue.pushFunction([=]() { m_instance.destroySurfaceKHR(m_surface); });

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

        m_deletionQueue.pushFunction([=]() { m_device.destroy(); });

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

        for (const auto& swapchainImageView : m_swapchainImageViews)
        {
            m_deletionQueue.pushFunction([=]() { m_device.destroyImageView(swapchainImageView); });
        }

        m_swapchainImageFormat = vk::Format(vkbSwapchain.image_format);

        // Get the command queue and family (i.e type of queue).
        m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_graphicsQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        m_transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
        m_transferQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

        // Create the command pools.

        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            // Specify that we much be able to reset individual command buffers created from this pool.
            // also, the commands recorded must be compatible with the graphics queue.
            const vk::CommandPoolCreateInfo commandPoolCreateInfo = {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = m_graphicsQueueIndex,
            };

            m_frameData[frameIndex].graphicsCommandPool = m_device.createCommandPool(commandPoolCreateInfo);
            m_deletionQueue.pushFunction([=]()
                                         { m_device.destroyCommandPool(m_frameData[frameIndex].graphicsCommandPool); });

            // Create the command buffer.

            // Specify it is primarily, implying it can be sent for execution on a queue. Secondary buffers act as
            // subcommands to a primary buffer.
            const vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
                .commandPool = m_frameData[frameIndex].graphicsCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1u,
            };

            m_frameData[frameIndex].graphicsCommandBuffer =
                m_device.allocateCommandBuffers(commandBufferAllocateInfo).at(0);
        }

        // Create command buffer and command pool but for transfer queue.
        const vk::CommandPoolCreateInfo transferCommandPoolCreateInfo = {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = m_transferQueueIndex,
        };

        m_transferCommandPool = m_device.createCommandPool(transferCommandPoolCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyCommandPool(m_transferCommandPool); });

        const vk::CommandBufferAllocateInfo transferCommandPoolAllocateInfo = {
            .commandPool = m_transferCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1u,
        };

        m_transferCommandBuffer = m_device.allocateCommandBuffers(transferCommandPoolAllocateInfo).at(0);

        // Create synchronization primitives.
        // Specify that the Signaled flag is set while creating the fence.
        // Base case for the first loop (CPU - GPU sync).
        const vk::FenceCreateInfo fenceCreateInfo = {.flags = vk::FenceCreateFlagBits::eSignaled};

        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            m_frameData[frameIndex].renderFence = m_device.createFence(fenceCreateInfo);
            m_deletionQueue.pushFunction([=]() { m_device.destroyFence(m_frameData[frameIndex].renderFence); });

            // Create semaphores (GPU - GPU sync).
            const vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
            m_frameData[frameIndex].renderSemaphore = m_device.createSemaphore(semaphoreCreateInfo);
            m_frameData[frameIndex].presentationSemaphore = m_device.createSemaphore(semaphoreCreateInfo);

            m_deletionQueue.pushFunction(
                [=]()
                {
                    m_device.destroySemaphore(m_frameData[frameIndex].renderSemaphore);
                    m_device.destroySemaphore(m_frameData[frameIndex].presentationSemaphore);
                });
        }

        // Initialize the vulkan memory allocator.
        const VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {
            .physicalDevice = m_physicalDevice,
            .device = m_device,
            .instance = m_instance,
        };

        vkCheck(vmaCreateAllocator(&vmaAllocatorCreateInfo, &m_vmaAllocator));
        m_deletionQueue.pushFunction([=]() { vmaDestroyAllocator(m_vmaAllocator); });
    }

    void Engine::initPipelines()
    {
        // Create the depth texture for use by the pipeline.
        const vk::Extent3D depthImageExtent = {
            .width = m_windowExtent.width,
            .height = m_windowExtent.height,
            .depth = 1,
        };

        // We do not intend to read the image from the CPU, so we use tilingOptimal to let the GPU decide the optimal
        // way to arrange the texture in the GPU.
        const vk::ImageCreateInfo depthImageCreateInfo = {
            .imageType = vk::ImageType::e2D,
            .format = m_depthImageFormat,
            .extent = depthImageExtent,
            .mipLevels = 1u,
            .arrayLayers = 1u,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
        };

        const VmaAllocationCreateInfo vmaDepthImageAllocationCreateInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        const VkImageCreateInfo vkDepthImageCreateInfo = depthImageCreateInfo;

        VkImage vkDepthImage{};
        vkCheck(vmaCreateImage(m_vmaAllocator,
                               &vkDepthImageCreateInfo,
                               &vmaDepthImageAllocationCreateInfo,
                               &vkDepthImage,
                               &m_depthImage.allocation,
                               nullptr));
        m_depthImage.image = vkDepthImage;

        m_deletionQueue.pushFunction(
            [=]()
            {
                const VkImage vkDepthImage = m_depthImage.image;
                vmaDestroyImage(m_vmaAllocator, vkDepthImage, m_depthImage.allocation);
            });

        const vk::ImageViewCreateInfo depthImageViewCreateInfo = {
            .image = m_depthImage.image,
            .viewType = vk::ImageViewType::e2D,
            .format = m_depthImageFormat,
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eDepth,
                    .baseMipLevel = 0u,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        // Create depth texture image view.
        m_depthImageView = m_device.createImageView(depthImageViewCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyImageView(m_depthImageView); });

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

        // There will be only one vertex binding which has per vertex input rate.
        // Vertex buffer will have float3 position, float3 normal, and float3 color for now.
        const vk::VertexInputBindingDescription mainVertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex,
        };

        const std::array<vk::VertexInputAttributeDescription, 3> vertexInputAttributeDescriptions = {
            vk::VertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, position),
            },

            vk::VertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, normal),
            },
            vk::VertexInputAttributeDescription{
                .location = 2,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(Vertex, color),
            },
        };

        const vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            .vertexBindingDescriptionCount = 1u,
            .pVertexBindingDescriptions = &mainVertexInputBindingDescription,
            .vertexAttributeDescriptionCount = 3u,
            .pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data(),
        };

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
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = false,
            .lineWidth = 1.0f,
        };

        // Setup depth stencil state.
        const vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .stencilTestEnable = false,
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

        // Setup viewport and scissor state (y is the bottom left, and height is -1 * actual height to replicate
        // DirectX's coordinate system).
        const vk::Viewport viewport = {
            .x = 0.0f,
            .y = static_cast<float>(m_windowExtent.height),
            .width = static_cast<float>(m_windowExtent.width),
            .height = -1 * static_cast<float>(m_windowExtent.height),
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

        // Create pipeline layout.
        const vk::PushConstantRange pushConstant = {
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0u,
            .size = sizeof(TransformBuffer),
        };

        const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .pushConstantRangeCount = 1u,
            .pPushConstantRanges = &pushConstant,
        };

        m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyPipelineLayout(m_pipelineLayout); });

        // Create the pipeline.
        const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
            vertexShaderStageCreateInfo,
            pixelShaderStageCreateInfo,
        };

        const vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            .colorAttachmentCount = 1u,
            .pColorAttachmentFormats = &m_swapchainImageFormat,
            .depthAttachmentFormat = m_depthImageFormat,
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
            .pDepthStencilState = &depthStencilStateCreateInfo,
            .pColorBlendState = &colorBlendStateCreateInfo,
            .pDynamicState = nullptr,
            .layout = m_pipelineLayout,
        };

        const auto trianglePipelineResult =
            m_device.createGraphicsPipeline(nullptr, triangleGraphicsPIpelineStateCreateInfo);

        vkCheck(trianglePipelineResult.result);

        m_pipeline = trianglePipelineResult.value;
        m_deletionQueue.pushFunction([=]() { m_device.destroyPipeline(m_pipeline); });

        // Destroy shader modules.
        m_device.destroyShaderModule(triangleVertexShaderModule);
        m_device.destroyShaderModule(trianglePixelShaderModule);
    }

    void Engine::initMeshes()
    {
        m_triangleMesh.vertices.resize(3);

        m_triangleMesh.vertices[0] = Vertex{.position = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}};
        m_triangleMesh.vertices[1] = Vertex{.position = {0.0f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f}};
        m_triangleMesh.vertices[2] = Vertex{.position = {0.5f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 1.0f}};

        const vk::BufferCreateInfo vertexBufferCreateInfo = {
            .size = m_triangleMesh.vertices.size() * sizeof(Vertex),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        m_triangleMesh.vertexBuffer = createGPUBuffer(vertexBufferCreateInfo, m_triangleMesh.vertices.data());
        m_suzanneMesh = createMesh("assets/Suzanne/glTF/Suzanne.gltf");
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
    }

    void Engine::render()
    {
        // Wait for the GPU to finish execution of the last frame. Max timeout is 1 second.
        vkCheck(m_device.waitForFences(1u, &getCurrentFrameData().renderFence, true, ONE_SECOND_IN_NANOSECOND));

        // Reset fence.
        vkCheck(m_device.resetFences(1u, &getCurrentFrameData().renderFence));

        // Request image from swapchain.
        // Signal the presentation semaphore when image is acquired.
        uint32_t swapchainImageIndex{};
        vkCheck(m_device.acquireNextImageKHR(m_swapchain,
                                             ONE_SECOND_IN_NANOSECOND,
                                             getCurrentFrameData().presentationSemaphore,
                                             {},
                                             &swapchainImageIndex));

        getCurrentFrameData().graphicsCommandBuffer.reset();

        vk::CommandBuffer& cmd = getCurrentFrameData().graphicsCommandBuffer;

        vk::CommandBufferBeginInfo commandBufferBeginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

        cmd.begin(commandBufferBeginInfo);

        // Aka the render target view clear value.
        const vk::ClearValue colorImageClearValue = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
        const vk::ClearValue depthImageClearValue = {.depthStencil{
            .depth = 1.0f,
            .stencil = 1u,
        }};

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
            .clearValue = colorImageClearValue,
        };

        const vk::RenderingAttachmentInfo depthAttachmentInfo = {
            .imageView = m_depthImageView,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = depthImageClearValue,
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
            .pDepthAttachment = &depthAttachmentInfo,
        };

        cmd.beginRendering(renderingInfo);

        static const math::XMVECTOR eyePosition = math::XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
        static const math::XMVECTOR targetPosition = math::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        static const math::XMVECTOR upDirection = math::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        // Setup push constant data.
        const TransformBuffer transformBufferData = {
            .modelMatrix = math::XMMatrixRotationY(m_frameNumber * 0.01f) * math::XMMatrixTranslation(0.0f, 0.0f, 5.0f),
            .viewProjectionMatrix =
                math::XMMatrixLookAtLH(eyePosition, targetPosition, upDirection) *
                math::XMMatrixPerspectiveFovLH(math::XMConvertToRadians(45.0f),
                                               (float)m_windowExtent.width / (float)m_windowExtent.height,
                                               0.1f,
                                               100.0f),
        };

        const vk::DeviceSize vertexBufferOffset = 0;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        cmd.bindVertexBuffers(0u, m_triangleMesh.vertexBuffer.buffer, vertexBufferOffset);
        cmd.pushConstants(m_pipelineLayout,
                          vk::ShaderStageFlagBits::eVertex,
                          0,
                          sizeof(TransformBuffer),
                          &transformBufferData);

        cmd.draw(3u, 1u, 0u, 0u);

        cmd.bindVertexBuffers(0u, m_suzanneMesh.vertexBuffer.buffer, vertexBufferOffset);
        cmd.draw(m_suzanneMesh.vertices.size(), 1u, 0u, 0u);

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
            .pWaitSemaphores = &getCurrentFrameData().presentationSemaphore,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1u,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1u,
            .pSignalSemaphores = &getCurrentFrameData().renderSemaphore,
        };

        vkCheck(m_graphicsQueue.submit(1u, &submitInfo, getCurrentFrameData().renderFence));

        // Setup for presentation.

        // Wait for the render semaphore to be signaled (will happen after commands submitted to the queue is
        // completed).
        const vk::PresentInfoKHR presentInfo = {
            .waitSemaphoreCount = 1u,
            .pWaitSemaphores = &getCurrentFrameData().renderSemaphore,
            .swapchainCount = 1u,
            .pSwapchains = &m_swapchain,
            .pImageIndices = &swapchainImageIndex,
        };

        vkCheck(m_graphicsQueue.presentKHR(presentInfo));
    }

    void Engine::cleanup()
    {
        // Cleanup is done in the reverse order of creation. Handled by deletion queue.
        m_device.waitIdle();
        m_graphicsQueue.waitIdle();

        m_deletionQueue.flush();
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

    Buffer Engine::createGPUBuffer(const vk::BufferCreateInfo bufferCreateInfo, const void* data)
    {
        // Create staging buffer (i.e in GPU / CPU shared memory).
        const vk::BufferCreateInfo stagingBufferCreateInfo = {
            .size = bufferCreateInfo.size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
        };

        VmaAllocationCreateInfo stagingBufferAllocationCreateInfo = {.usage = VMA_MEMORY_USAGE_CPU_TO_GPU};

        VkBuffer stagingBuffer{};
        VmaAllocation stagingBufferAllocation{};

        const VkBufferCreateInfo vkStagingBufferCreateInfo = stagingBufferCreateInfo;

        vkCheck(vmaCreateBuffer(m_vmaAllocator,
                                &vkStagingBufferCreateInfo,
                                &stagingBufferAllocationCreateInfo,
                                &stagingBuffer,
                                &stagingBufferAllocation,
                                nullptr));

        // Get a pointer to this memory allocation and copy the data passed into the function to this allocation.
        void* dataPtr{};
        vkCheck(vmaMapMemory(m_vmaAllocator, stagingBufferAllocation, &dataPtr));
        std::memcpy(dataPtr, data, stagingBufferCreateInfo.size);
        vmaUnmapMemory(m_vmaAllocator, stagingBufferAllocation);

        // Create buffer (GPU only memory), that will be returned by the function.
        Buffer buffer{};

        const VmaAllocationCreateInfo vmaAllocationCreateInfo = {.usage = VMA_MEMORY_USAGE_GPU_ONLY};

        const VkBufferCreateInfo vkBufferCreateInfo = bufferCreateInfo;

        VkBuffer vkBuffer{};
        vkCheck(vmaCreateBuffer(m_vmaAllocator,
                                &vkBufferCreateInfo,
                                &vmaAllocationCreateInfo,
                                &vkBuffer,
                                &buffer.allocation,
                                nullptr));

        buffer.buffer = vkBuffer;

        // Reset the transfer command buffer and begin it.
        m_transferCommandBuffer.reset();

        const vk::CommandBufferBeginInfo transferCommandBufferBeginInfo = {
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
        };

        m_transferCommandBuffer.begin(transferCommandBufferBeginInfo);

        // Copy the data of staging buffer to the GPU only buffer.
        const vk::BufferCopy bufferCopyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bufferCreateInfo.size,
        };

        m_transferCommandBuffer.copyBuffer(stagingBuffer, buffer.buffer, 1u, &bufferCopyRegion);

        m_transferCommandBuffer.end();

        // Prepare for submission.
        const vk::SubmitInfo transferSubmitInfo = {
            .commandBufferCount = 1u,
            .pCommandBuffers = &m_transferCommandBuffer,
        };

        m_transferQueue.submit(transferSubmitInfo);
        m_transferQueue.waitIdle();

        // Cleanup the temporary staging buffer and its allocation.
        vmaDestroyBuffer(m_vmaAllocator, stagingBuffer, stagingBufferAllocation);

        m_deletionQueue.pushFunction([=]() { vmaDestroyBuffer(m_vmaAllocator, buffer.buffer, buffer.allocation); });

        return buffer;
    }

    Mesh Engine::createMesh(const std::string_view modelPath)
    {
        // Use tinygltf loader to load the model.

        const std::string fullModelPath = m_rootDirectory + modelPath.data();

        std::string warning{};
        std::string error{};

        tinygltf::TinyGLTF context{};

        tinygltf::Model model{};

        if (!context.LoadASCIIFromFile(&model, &error, &warning, fullModelPath))
        {
            if (!error.empty())
            {
                fatalError(error);
            }

            if (!warning.empty())
            {
                fatalError(warning);
            }
        }

        // Build meshes.
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];

        tinygltf::Node& node = model.nodes[0u];
        node.mesh = std::max<int32_t>(0u, node.mesh);

        const tinygltf::Mesh& nodeMesh = model.meshes[node.mesh];

        // note(rtarun9) : for now have all vertices in single vertex buffer.
        std::vector<Vertex> vertices{};

        for (size_t i = 0; i < nodeMesh.primitives.size(); ++i)
        {
            std::vector<uint32_t> indices{};

            // Reference used :
            // https://github.com/mateeeeeee/Adria-DX12/blob/fc98468095bf5688a186ca84d94990ccd2f459b0/Adria/Rendering/EntityLoader.cpp.

            // Get Accessors, buffer view and buffer for each attribute (position, textureCoord, normal).
            tinygltf::Primitive primitive = nodeMesh.primitives[i];
            const tinygltf::Accessor& indexAccesor = model.accessors[primitive.indices];

            // Position data.
            const tinygltf::Accessor& positionAccesor = model.accessors[primitive.attributes["POSITION"]];
            const tinygltf::BufferView& positionBufferView = model.bufferViews[positionAccesor.bufferView];
            const tinygltf::Buffer& positionBuffer = model.buffers[positionBufferView.buffer];

            const int positionByteStride = positionAccesor.ByteStride(positionBufferView);
            uint8_t const* const positions =
                &positionBuffer.data[positionBufferView.byteOffset + positionAccesor.byteOffset];

            // TextureCoord data.
            const tinygltf::Accessor& textureCoordAccesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
            const tinygltf::BufferView& textureCoordBufferView = model.bufferViews[textureCoordAccesor.bufferView];
            const tinygltf::Buffer& textureCoordBuffer = model.buffers[textureCoordBufferView.buffer];
            const int textureCoordBufferStride = textureCoordAccesor.ByteStride(textureCoordBufferView);
            uint8_t const* const texcoords =
                &textureCoordBuffer.data[textureCoordBufferView.byteOffset + textureCoordAccesor.byteOffset];

            // Normal data.
            const tinygltf::Accessor& normalAccesor = model.accessors[primitive.attributes["NORMAL"]];
            const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccesor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
            const int normalByteStride = normalAccesor.ByteStride(normalBufferView);
            uint8_t const* const normals = &normalBuffer.data[normalBufferView.byteOffset + normalAccesor.byteOffset];

            // Fill in the vertices's array.
            for (size_t i : std::views::iota(0u, positionAccesor.count))
            {
                const DirectX::XMFLOAT3 position{
                    (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[0],
                    (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[1],
                    (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[2]};

                const DirectX::XMFLOAT2 textureCoord{
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[0],
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[1],
                };

                const DirectX::XMFLOAT3 normal{
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[0],
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[1],
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[2],
                };

                // note(rtarun9) : using normals as colors for now, until texture loading is implemented.
                vertices.emplace_back(Vertex{position, normal, normal});
            }

            // Get the index buffer data.
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccesor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const int indexByteStride = indexAccesor.ByteStride(indexBufferView);
            uint8_t const* const indexes =
                indexBuffer.data.data() + indexBufferView.byteOffset + indexAccesor.byteOffset;

            // Fill indices array.
            for (size_t i : std::views::iota(0u, indexAccesor.count))
            {
                if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    indices.push_back(
                        static_cast<uint32_t>((reinterpret_cast<uint16_t const*>(indexes + (i * indexByteStride)))[0]));
                }
                else if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    indices.push_back(
                        static_cast<uint32_t>((reinterpret_cast<uint32_t const*>(indexes + (i * indexByteStride)))[0]));
                }
            }
        }

        Mesh mesh{};
        mesh.vertices = std::move(vertices);

        const vk::BufferCreateInfo vertexBufferCreateInfo = {
            .size = sizeof(Vertex) * mesh.vertices.size(),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        mesh.vertexBuffer = createGPUBuffer(vertexBufferCreateInfo, mesh.vertices.data());

        return mesh;
    }
}
