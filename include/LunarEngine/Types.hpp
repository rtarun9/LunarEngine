#pragma once

#include "Resources.hpp"

namespace lunar
{
    struct DeletionQueue
    {
        std::deque<std::function<void()>> functions{};

        void pushFunction(std::function<void()>&& func) { functions.push_back(func); }

        void flush()
        {
            for (auto it = functions.rbegin(); it != functions.rend(); ++it)
            {
                (*it)();
            }

            functions.clear();
        }
    };

    // Vertex / Mesh related.
    struct Vertex
    {
        math::XMFLOAT3 position{};
        math::XMFLOAT3 normal{};
        math::XMFLOAT3 color{};

        // Returns the vertex input state for this specific vertex type.
        [[nodiscard]] static vk::PipelineVertexInputStateCreateInfo getVertexInputState()
        {
            // There will be a single binding with per vertex input rate.
            static const vk::VertexInputBindingDescription vertexInputBindingDescription = {
                .binding = 0u,
                .stride = sizeof(math::XMFLOAT3) * 3,
                .inputRate = vk::VertexInputRate::eVertex,
            };

            // Setup the vertex attributes.
            static const std::array<vk::VertexInputAttributeDescription, 3> vertexInputAttributes = {vk::VertexInputAttributeDescription{
                                                                                                         .location = 0u,
                                                                                                         .binding = 0u,
                                                                                                         .format = vk::Format::eR32G32B32Sfloat,
                                                                                                         .offset = offsetof(Vertex, position),
                                                                                                     },
                                                                                                     vk::VertexInputAttributeDescription{
                                                                                                         .location = 1u,
                                                                                                         .binding = 0u,
                                                                                                         .format = vk::Format::eR32G32B32Sfloat,
                                                                                                         .offset = offsetof(Vertex, normal),
                                                                                                     },
                                                                                                     vk::VertexInputAttributeDescription{
                                                                                                         .location = 2u,
                                                                                                         .binding = 0u,
                                                                                                         .format = vk::Format::eR32G32B32Sfloat,
                                                                                                         .offset = offsetof(Vertex, color),
                                                                                                     }};

            static const vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
                .vertexBindingDescriptionCount = 1u,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 3u,
                .pVertexAttributeDescriptions = vertexInputAttributes.data(),
            };

            return vertexInputStateCreateInfo;
        }
    };

    struct Mesh
    {
        uint32_t indicesCount{};
        Buffer vertexBuffer{};
        Buffer indexBuffer{};
    };

    struct SceneBufferData
    {
        math::XMMATRIX viewProjectionMatrix{};
    };

    struct TransformBufferData
    {
        math::XMMATRIX modelMatrix{math::XMMatrixIdentity()};
    };

    struct TransformBuffer
    {
        Buffer buffer{};
        TransformBufferData bufferData{};
    };

    struct BufferUploadData
    {
        Buffer stagingBuffer{};
        uint64_t size{};
    };

    struct Material
    {
        vk::Pipeline pipeline{};
        vk::PipelineLayout pipelineLayout{};
    };

    // Point to the mesh / material from the mesh / material collection.
    struct RenderObject
    {
        Mesh* mesh{};
        Material* material{};

        TransformBuffer transformBuffer{};
    };

    // Pipeline related.
    struct PipelineCreationDesc
    {
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};
        vk::PipelineVertexInputStateCreateInfo vertexInputState{};
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        vk::PipelineViewportStateCreateInfo viewportState{};
        vk::PipelineRasterizationStateCreateInfo rasterizationState{};
        vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
        vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    };

}
