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

        // Setup and create the descriptors.
        initDescriptors();

        // Setup the pipelines.
        initPipelines();

        // Initialize all meshes.
        initMeshes();

        // Initialize scene (i.e all render objects).
        initScene();

        // Upload buffers (all GPU only buffers will have data copied from a staging buffer and placed in their GPU
        // only memory).
        uploadBuffers();
    }

    void Engine::initVulkan()
    {
        // Initialize the core vulkan objects.

        // Get the instance and enable validation layers.
        // Creating a instance initializes the Vulkan library and lets the application tell information about itself
        // (only applicable if a AAA Game Engine and such).

        vkb::InstanceBuilder instanceBuilder{};
        const auto vkbInstanceResult =
            instanceBuilder.set_app_name("Lunar Engine").request_validation_layers(LUNAR_DEBUG).use_default_debug_messenger().require_api_version(1, 3, 0).build();
        if (!vkbInstanceResult)
        {
            fatalError("Failed to create vulkan instance.");
        }

        const vkb::Instance vkbInstance = vkbInstanceResult.value();
        m_instance = vkbInstance.instance;
        m_deletionQueue.pushFunction([=]() { vkDestroyInstance(m_instance, nullptr); });

        // Get the debug messenger.
        m_debugMessenger = vkbInstance.debug_messenger;

        m_deletionQueue.pushFunction([=]() { vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger, nullptr); });

        // Get the surface of the window opened by SDL (i.e get the underlying native platform surface). Required for
        // selection of physical device as the GPU must be able to render to the window.
        VkSurfaceKHR surface{};
        SDL_Vulkan_CreateSurface(m_window, m_instance, &surface);
        m_surface = surface;

        m_deletionQueue.pushFunction([=]() { m_instance.destroySurfaceKHR(m_surface); });

        // Specify that we require Vulkan 1.3's dynamic rendering feature.
        const vk::PhysicalDeviceVulkan13Features features{
            .synchronization2 = true,
            .dynamicRendering = true,
        };

        // Get the physical adapter that can render to the surface. Prefer discrete GPU's.
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

        // Initialize the vulkan memory allocator.
        const VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {
            .physicalDevice = m_physicalDevice,
            .device = m_device,
            .instance = m_instance,
        };

        vkCheck(vmaCreateAllocator(&vmaAllocatorCreateInfo, &m_vmaAllocator));
        m_deletionQueue.pushFunction([=]() { vmaDestroyAllocator(m_vmaAllocator); });

        // Get the command queue and family (i.e type of queue).
        // The family indicates what is the capabilities supported by that family of queues (ex family at index 0 may
        // support graphics, compute and transfer operations, and may have 2+ queues in it, while queue family at index
        // 1 supports only transfer workloads and has only 1 queue internally).
        m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_graphicsQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        m_transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
        m_transferQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

        initSwapchain();
        initCommandObjects();
        initSyncPrimitives();
    }

    void Engine::initSwapchain()
    {

        // Initialize the swapchain and get the swapchain images and image views.
        // Swapchain provides ability to store and render the rendering results to a surface.
        // Use FIFO_RELAXED_KHR, which caps the frame rate but if under performing (ex display is 60HZ but FPS is <60,
        // allows tearing).
        vkb::SwapchainBuilder vkbSwapchainBuilder{m_physicalDevice, m_device, m_surface};
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
        vkCheck(vmaCreateImage(m_vmaAllocator, &vkDepthImageCreateInfo, &vmaDepthImageAllocationCreateInfo, &vkDepthImage, &m_depthImage.allocation, nullptr));
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
    }

    void Engine::initCommandObjects()
    {
        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            // Create the command pools (i.e background allocators for command buffers).
            // Specify that we much be able to reset individual command buffers created from this pool.
            // also, the commands recorded must be compatible with the graphics queue.
            const vk::CommandPoolCreateInfo commandPoolCreateInfo = {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = m_graphicsQueueIndex,
            };

            m_frameData[frameIndex].graphicsCommandPool = m_device.createCommandPool(commandPoolCreateInfo);
            m_deletionQueue.pushFunction([=]() { m_device.destroyCommandPool(m_frameData[frameIndex].graphicsCommandPool); });

            // Create the command buffer.

            // Specify it is primarily, implying it can be sent for execution on a queue. Secondary buffers act as
            // subcommand buffers to a primary buffer.
            const vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
                .commandPool = m_frameData[frameIndex].graphicsCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1u,
            };

            m_frameData[frameIndex].graphicsCommandBuffer = m_device.allocateCommandBuffers(commandBufferAllocateInfo).at(0);
        }

        // Create command buffer and command pool but for transfer operations.
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

        // Reset the command buffer immediately, as it will be used to handle buffer copies.
        const vk::CommandBufferBeginInfo transferCommandBufferBeginInfo = {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };

        m_transferCommandBuffer.begin(transferCommandBufferBeginInfo);
    }

    void Engine::initSyncPrimitives()
    {
        // Create synchronization primitives.

        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            // Specify that the Signaled flag is set while creating the fence.
            // Base case for the first loop (CPU - GPU sync).

            const vk::FenceCreateInfo fenceCreateInfo = {.flags = vk::FenceCreateFlagBits::eSignaled};
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
    }

    void Engine::initDescriptors()
    {
        // Create descriptor pool. Maintains a pool of descriptors, from which descriptor sets are allocated.
        // Reserve 10 uniform buffer pointers.

        const std::array<vk::DescriptorPoolSize, 1u> descriptorPoolSizes = {
            {vk::DescriptorType::eUniformBuffer, 10},
        };

        // 10 descriptor sets can be allocated from this pool.
        const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            .maxSets = 10u,
            .poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
            .pPoolSizes = descriptorPoolSizes.data(),
        };

        m_descriptorPool = m_device.createDescriptorPool(descriptorPoolCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyDescriptorPool(m_descriptorPool); });

        // Descriptors are essentially pointers to a resource + some additional information about it. A descriptor set
        // is a set of descriptors. For performance, use set 0 as global, set 1 as per pass, set 2 as material and set 3
        // as per object (i.e inner loops will be binding only sets 2 and 3 will 0 and 1 will be less frequency unbound
        // and bound. Descriptor set layout gives the general shape / layout of the descriptor sets.

        // Setup the descriptor set layout. At binding 0, there will be 1 uniform buffers for use by the vertex shader (SceneBuffer).
        const std::array<vk::DescriptorSetLayoutBinding, 1> descriptorSetLayoutBindings = {
            vk::DescriptorSetLayoutBinding{
                .binding = 0u,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1u,
                .stageFlags = vk::ShaderStageFlagBits::eVertex,
            },
        };

        const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            .bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size()),
            .pBindings = descriptorSetLayoutBindings.data(),
        };

        m_globalDescriptorSetLayout = m_device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyDescriptorSetLayout(m_globalDescriptorSetLayout); });

        //  Setup descriptor sets.
        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            const vk::BufferCreateInfo sceneBufferCreateInfo = {.size = sizeof(SceneBufferData), .usage = vk::BufferUsageFlagBits::eUniformBuffer};

            m_frameData[frameIndex].sceneBuffer = createGPUBuffer(sceneBufferCreateInfo);

            // Allocate a descriptor set for the scene buffer for this frame.
            const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo = {
                .descriptorPool = m_descriptorPool,
                .descriptorSetCount = 1u,
                .pSetLayouts = &m_globalDescriptorSetLayout,
            };

            m_frameData[frameIndex].globalDescriptorSet = m_device.allocateDescriptorSets(descriptorSetAllocateInfo).at(0);

            // At this point, the descriptor set is allocated. Now, make the descriptor point to the camera buffer.
            const vk::DescriptorBufferInfo cameraDescriptorBufferInfo = {
                .buffer = m_frameData[frameIndex].sceneBuffer.buffer,
                .offset = 0u,
                .range = sizeof(SceneBufferData),
            };

            const vk::WriteDescriptorSet descriptorSetWrite = {
                .dstSet = m_frameData[frameIndex].globalDescriptorSet,
                .dstBinding = 0u,
                .descriptorCount = 1u,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &cameraDescriptorBufferInfo,
            };

            m_device.updateDescriptorSets(1u, &descriptorSetWrite, 0u, nullptr);
        }
    }

    void Engine::initPipelines()
    {
        // Create a simple pipeline.

        // Create shader modules.
        const vk::ShaderModule vertexShaderModule = createShaderModule("shaders/ShaderVS.cso");
        const vk::ShaderModule pixelShaderModule = createShaderModule("shaders/ShaderPS.cso");

        // Create pipeline shader stages.
        const vk::PipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertexShaderModule,
            .pName = "VsMain",
        };

        const vk::PipelineShaderStageCreateInfo pixelShaderStageCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = pixelShaderModule,
            .pName = "PsMain",
        };

        // Setup input state.
        const vk::PipelineVertexInputStateCreateInfo vertexInputState = Vertex::getVertexInputState();

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

        // Create material (i.e pipeline layout + pipeline).

        // Setup push constants.
        const vk::PushConstantRange transformBufferPushConstant = {
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0u,
            .size = sizeof(TransformBufferData),
        };

        // Create pipeline layout.
        const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .setLayoutCount = 1u,
            .pSetLayouts = &m_globalDescriptorSetLayout,
            .pushConstantRangeCount = 1u,
            .pPushConstantRanges = &transformBufferPushConstant,
        };

        // Create base pipeline layout.
        m_materials["BaseMaterial"].pipelineLayout = m_device.createPipelineLayout(pipelineLayoutCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyPipelineLayout(m_materials["BaseMaterial"].pipelineLayout); });

        const vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            .colorAttachmentCount = 1u,
            .pColorAttachmentFormats = &m_swapchainImageFormat,
            .depthAttachmentFormat = m_depthImageFormat,
        };

        // Create the pipeline.
        const PipelineCreationDesc pipelineCreationDesc = {
            .shaderStages = {vertexShaderStageCreateInfo, pixelShaderStageCreateInfo},
            .vertexInputState = vertexInputState,
            .inputAssemblyState = inputAssemblyStateCreateInfo,
            .viewportState = viewportStateCreateInfo,
            .rasterizationState = rasterizationStateCreateInfo,
            .depthStencilState = depthStencilStateCreateInfo,
            .pipelineRenderingInfo = pipelineRenderingCreateInfo,
        };

        m_materials["BaseMaterial"].pipeline = createPipeline(pipelineCreationDesc, m_materials["BaseMaterial"].pipelineLayout);
    }

    void Engine::initMeshes()
    {

        std::vector<Vertex> triangleVertices(3);

        triangleVertices[0] = Vertex{.position = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}};
        triangleVertices[1] = Vertex{.position = {0.0f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f}};
        triangleVertices[2] = Vertex{.position = {0.5f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 1.0f}};

        const vk::BufferCreateInfo vertexBufferCreateInfo = {
            .size = triangleVertices.size() * sizeof(Vertex),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        m_meshes["Triangle"].vertexBuffer = createGPUBuffer(vertexBufferCreateInfo, triangleVertices.data());

        std::vector<uint32_t> triangleIndices{0u, 1u, 2u};

        const vk::BufferCreateInfo indexBufferCreateInfo = {
            .size = triangleIndices.size() * sizeof(uint32_t),
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        m_meshes["Triangle"].indexBuffer = createGPUBuffer(indexBufferCreateInfo, triangleIndices.data());
        m_meshes["Triangle"].indicesCount = triangleIndices.size();

        m_meshes["Suzanne"] = createMesh("assets/Suzanne/glTF/Suzanne.gltf");
    }

    void Engine::initScene()
    {
        const vk::BufferCreateInfo transformBufferCreateInfo = {
            .size = sizeof(TransformBufferData),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
        };

        RenderObject triangle = {
            .mesh = &m_meshes["Triangle"],
            .material = &m_materials["BaseMaterial"],
            .transformBuffer = {.buffer = createGPUBuffer(transformBufferCreateInfo)},
        };

        m_renderObjects.emplace_back(triangle);

        RenderObject suzanne = {
            .mesh = &m_meshes["Suzanne"],
            .material = &m_materials["BaseMaterial"],
            .transformBuffer = {.buffer = createGPUBuffer(transformBufferCreateInfo)},
        };

        m_renderObjects.emplace_back(suzanne);
    }

    void Engine::run()
    {
        // Initialize SDL and the graphics back end.
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
        // Wait for the GPU to finish execution of commands previously submitted to the queue for this frame.
        vkCheck(m_device.waitForFences(1u, &getCurrentFrameData().renderFence, true, ONE_SECOND_IN_NANOSECOND));

        // Reset fence.
        vkCheck(m_device.resetFences(1u, &getCurrentFrameData().renderFence));

        // Request image from swapchain.
        // Signal the presentation semaphore when image is acquired. Only after a image is acquired we can present the
        // rendered image. Block the main thread for the timeout duration if we cannot acquire swapchain image for
        // rendering.
        uint32_t swapchainImageIndex{};
        vkCheck(m_device.acquireNextImageKHR(m_swapchain, ONE_SECOND_IN_NANOSECOND, getCurrentFrameData().presentationSemaphore, {}, &swapchainImageIndex));

        getCurrentFrameData().graphicsCommandBuffer.reset();

        vk::CommandBuffer& cmd = getCurrentFrameData().graphicsCommandBuffer;

        vk::CommandBufferBeginInfo commandBufferBeginInfo = {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};

        cmd.begin(commandBufferBeginInfo);

        // Set clear values for the color image and the depth image.
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

        // Before the color attachment output is required by pipeline, the image must transition into the
        // colorAttachmentOutput format. Else, the pipeline stage execution will be blocked.
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eNone,
                            vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::DependencyFlagBits::eByRegion,
                            0u,
                            nullptr,
                            0u,
                            nullptr,
                            1u,
                            &imageToAttachmentBarrier);

        // Specify rendering attachments (color and depth images).
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

        // Setup scene buffer data.
        static const math::XMVECTOR eyePosition = math::XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
        static const math::XMVECTOR targetPosition = math::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        static const math::XMVECTOR upDirection = math::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        const SceneBufferData sceneBufferData = {
            .viewProjectionMatrix = math::XMMatrixLookAtLH(eyePosition, targetPosition, upDirection) *
                                    math::XMMatrixPerspectiveFovLH(math::XMConvertToRadians(45.0f), (float)m_windowExtent.width / (float)m_windowExtent.height, 0.1f, 100.0f),
        };

        // Update scene buffer.
        void* data{};
        vkCheck(vmaMapMemory(m_vmaAllocator, getCurrentFrameData().sceneBuffer.allocation, &data));
        std::memcpy(data, &sceneBufferData, sizeof(SceneBufferData));
        vmaUnmapMemory(m_vmaAllocator, getCurrentFrameData().sceneBuffer.allocation);

        // Update transform buffers (WILL BE MOVED SOON).

        // Triangle transform buffer.
        m_renderObjects[0].transformBuffer.bufferData.modelMatrix = math::XMMatrixRotationY(sin(m_frameNumber / 120.0f)) * math::XMMatrixTranslation(-2.0f, 0.0f, 0.0f);

        // Suzanne transform buffer.
        m_renderObjects[1].transformBuffer.bufferData.modelMatrix = math::XMMatrixRotationY((m_frameNumber / 60.0f)) * math::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

        // Update material and mesh only if the current render object's material / mesh is different from the one previously used.
        // Useful as binding pipelines unnecessarily is not the most efficient.
        Material* lastMaterial = nullptr;
        Mesh* lastMesh = nullptr;

        for (const auto& renderObject : m_renderObjects)
        {
            if (renderObject.material != lastMaterial)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, renderObject.material->pipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderObject.material->pipelineLayout, 0u, 1u, &getCurrentFrameData().globalDescriptorSet, 0u, nullptr);

                lastMaterial = renderObject.material;
            }

            if (renderObject.mesh != lastMesh)
            {
                constexpr vk::DeviceSize vertexBufferOffset = 0;
                constexpr vk::DeviceSize indexBufferOffset = 0;

                cmd.bindVertexBuffers(0u, renderObject.mesh->vertexBuffer.buffer, vertexBufferOffset);
                cmd.bindIndexBuffer(renderObject.mesh->indexBuffer.buffer, indexBufferOffset, vk::IndexType::eUint32);

                lastMesh = renderObject.mesh;
            }

            cmd.pushConstants(lastMaterial->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0u, sizeof(TransformBufferData), &renderObject.transformBuffer.bufferData);
            cmd.drawIndexed(lastMesh->indicesCount, 1u, 0u, 0u, 0u);
        }

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

        // ???
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
        m_transferQueue.waitIdle();

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
        const std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        // Place file pointer to the beginning.
        shaderBytecodeFile.seekg(0);

        shaderBytecodeFile.read((char*)(buffer.data()), fileSize);

        shaderBytecodeFile.close();

        // Create the shader module. Size must be in bytes.
        const vk::ShaderModuleCreateInfo shaderModuleCreateInfo = {
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data(),
        };

        const vk::ShaderModule shaderModule = m_device.createShaderModule(shaderModuleCreateInfo);
        m_deletionQueue.pushFunction([=]() { m_device.destroyShaderModule(shaderModule); });

        return shaderModule;
    }

    Buffer Engine::createGPUBuffer(const vk::BufferCreateInfo bufferCreateInfo, const void* data)
    {
        Buffer buffer{};

        // if data is != nullptr, create a staging buffer and upload the data to a GPU only buffer.
        if (data)
        {
            // Create staging buffer (i.e in GPU / CPU shared memory).
            const vk::BufferCreateInfo stagingBufferCreateInfo = {
                .size = bufferCreateInfo.size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
            };

            const VmaAllocationCreateInfo stagingBufferAllocationCreateInfo = {.usage = VMA_MEMORY_USAGE_CPU_TO_GPU};

            Buffer stagingBuffer{};

            VkBuffer vkStagingBuffer{};

            const VkBufferCreateInfo vkStagingBufferCreateInfo = stagingBufferCreateInfo;

            vkCheck(vmaCreateBuffer(m_vmaAllocator, &vkStagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &vkStagingBuffer, &stagingBuffer.allocation, nullptr));

            stagingBuffer.buffer = vkStagingBuffer;

            // Get a pointer to this memory allocation and copy the data passed into the function to this allocation.
            void* dataPtr{};
            vkCheck(vmaMapMemory(m_vmaAllocator, stagingBuffer.allocation, &dataPtr));
            std::memcpy(dataPtr, data, stagingBufferCreateInfo.size);
            vmaUnmapMemory(m_vmaAllocator, stagingBuffer.allocation);

            // Create buffer (GPU only memory), that will be returned by the function.

            const VmaAllocationCreateInfo vmaAllocationCreateInfo = {.usage = VMA_MEMORY_USAGE_GPU_ONLY};

            const VkBufferCreateInfo vkBufferCreateInfo = bufferCreateInfo;

            VkBuffer vkBuffer{};
            vkCheck(vmaCreateBuffer(m_vmaAllocator, &vkBufferCreateInfo, &vmaAllocationCreateInfo, &vkBuffer, &buffer.allocation, nullptr));

            buffer.buffer = vkBuffer;

            // Issue a copy command to transfer data for CPU - GPU shared memory to GPU only memory.
            const vk::BufferCopy copyRegions = {
                .srcOffset = 0u,
                .dstOffset = 0u,
                .size = bufferCreateInfo.size,
            };

            m_transferCommandBuffer.copyBuffer(stagingBuffer.buffer, buffer.buffer, 1u, &copyRegions);

            m_uploadBufferDeletionQueue.pushFunction(
                [=]()
                {
                    VkBuffer buffer = stagingBuffer.buffer;
                    vmaDestroyBuffer(m_vmaAllocator, buffer, stagingBuffer.allocation);
                });
        }

        // if data is a nullptr, create a buffer in CPU / GPU shared memory.
        else
        {
            const VmaAllocationCreateInfo vmaBufferAllocationCreateInfo = {.usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
            const VkBufferCreateInfo vkBufferCreateInfo = bufferCreateInfo;

            VkBuffer vkBuffer{};
            vkCheck(vmaCreateBuffer(m_vmaAllocator, &vkBufferCreateInfo, &vmaBufferAllocationCreateInfo, &vkBuffer, &buffer.allocation, nullptr));

            buffer.buffer = vkBuffer;
        }

        m_deletionQueue.pushFunction([=]() { vmaDestroyBuffer(m_vmaAllocator, buffer.buffer, buffer.allocation); });

        return buffer;
    }

    void Engine::uploadBuffers()
    {

        // Prepare for submission of transfer command buffer to execute all buffer copy commands.
        m_transferCommandBuffer.end();

        const vk::SubmitInfo transferSubmitInfo = {
            .commandBufferCount = 1u,
            .pCommandBuffers = &m_transferCommandBuffer,
        };

        m_transferQueue.submit(transferSubmitInfo);
        m_transferQueue.waitIdle();

        m_uploadBufferDeletionQueue.flush();
    }

    vk::Pipeline Engine::createPipeline(const PipelineCreationDesc& pipelineCreationDesc, const vk::PipelineLayout& pipelineLayout)
    {
        // Setup state that will not be used for now and are not part of the pipeline creation desc.
        const vk::PipelineMultisampleStateCreateInfo multisampleStateCreatInfo{};

        const vk::PipelineColorBlendAttachmentState colorBlendAttachmentState = {
            .blendEnable = false,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        const vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
            .logicOpEnable = false,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1u,
            .pAttachments = &colorBlendAttachmentState,
        };

        // Setup the graphics pipeline state create info.
        const vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            .pNext = &pipelineCreationDesc.pipelineRenderingInfo,
            .stageCount = static_cast<uint32_t>(pipelineCreationDesc.shaderStages.size()),
            .pStages = pipelineCreationDesc.shaderStages.data(),
            .pVertexInputState = &pipelineCreationDesc.vertexInputState,
            .pInputAssemblyState = &pipelineCreationDesc.inputAssemblyState,
            .pViewportState = &pipelineCreationDesc.viewportState,
            .pRasterizationState = &pipelineCreationDesc.rasterizationState,
            .pMultisampleState = &multisampleStateCreatInfo,
            .pDepthStencilState = &pipelineCreationDesc.depthStencilState,
            .pColorBlendState = &colorBlendStateCreateInfo,
            .layout = pipelineLayout,
        };

        const auto result = m_device.createGraphicsPipeline({}, graphicsPipelineCreateInfo);
        vkCheck(result.result);

        const vk::Pipeline pipeline = result.value;
        m_deletionQueue.pushFunction([=]() { m_device.destroyPipeline(pipeline); });

        return pipeline;
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

        // note(rtarun9) : for now have all vertices in single vertex buffer. Same for index buffer.
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        for (size_t i = 0; i < nodeMesh.primitives.size(); ++i)
        {

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
            uint8_t const* const positions = &positionBuffer.data[positionBufferView.byteOffset + positionAccesor.byteOffset];

            // TextureCoord data.
            const tinygltf::Accessor& textureCoordAccesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
            const tinygltf::BufferView& textureCoordBufferView = model.bufferViews[textureCoordAccesor.bufferView];
            const tinygltf::Buffer& textureCoordBuffer = model.buffers[textureCoordBufferView.buffer];
            const int textureCoordBufferStride = textureCoordAccesor.ByteStride(textureCoordBufferView);
            uint8_t const* const texcoords = &textureCoordBuffer.data[textureCoordBufferView.byteOffset + textureCoordAccesor.byteOffset];

            // Normal data.
            const tinygltf::Accessor& normalAccesor = model.accessors[primitive.attributes["NORMAL"]];
            const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccesor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
            const int normalByteStride = normalAccesor.ByteStride(normalBufferView);
            uint8_t const* const normals = &normalBuffer.data[normalBufferView.byteOffset + normalAccesor.byteOffset];

            // Fill in the vertices's array.
            for (size_t i : std::views::iota(0u, positionAccesor.count))
            {
                const math::XMFLOAT3 position{(reinterpret_cast<float const*>(positions + (i * positionByteStride)))[0],
                                              (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[1],
                                              (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[2]};

                const math::XMFLOAT2 textureCoord{
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[0],
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[1],
                };

                const math::XMFLOAT3 normal{
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
            uint8_t const* const indexes = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccesor.byteOffset;

            // Fill indices array.
            for (size_t i : std::views::iota(0u, indexAccesor.count))
            {
                if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint16_t const*>(indexes + (i * indexByteStride)))[0]));
                }
                else if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint32_t const*>(indexes + (i * indexByteStride)))[0]));
                }
            }
        }

        Mesh mesh{};
        mesh.indicesCount = indices.size();

        const vk::BufferCreateInfo vertexBufferCreateInfo = {
            .size = sizeof(Vertex) * vertices.size(),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        mesh.vertexBuffer = createGPUBuffer(vertexBufferCreateInfo, vertices.data());

        const vk::BufferCreateInfo indexBufferCreateInfo = {
            .size = sizeof(uint32_t) * indices.size(),
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        };

        mesh.indexBuffer = createGPUBuffer(indexBufferCreateInfo, indices.data());

        return mesh;
    }
}
