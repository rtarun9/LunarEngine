#pragma once

namespace lunar
{
    struct Vertex
    {
        math::XMFLOAT3 position{};
        math::XMFLOAT3 normal{};
        math::XMFLOAT3 color{};
    };

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

    struct Mesh
    {
        std::vector<Vertex> vertices{};
        Buffer vertexBuffer{};
    };

    struct TransformBuffer
    {
        math::XMMATRIX modelMatrix{};
        math::XMMATRIX viewProjectionMatrix{};
    };

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
}