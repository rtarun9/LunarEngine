#pragma once

inline void fatalError(const std::string_view message, const std::source_location source_location = std::source_location::current())
{
    const std::string errorMessage =
        message.data() +
        std::format(" Source Location data : File Name -> {}, Function Name -> {}, Line Number -> {}, Column -> {}", source_location.file_name(), source_location.function_name(), source_location.line(), source_location.column());

    throw std::runtime_error(errorMessage.data());
}

inline void vkCheck(const vk::Result& result)
{
    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error(vk::to_string(result));
    }
}

inline void vkCheck(const VkResult& result)
{
    if (result != VK_SUCCESS)
    {
        const vk::Result res = vk::Result(result);
        throw std::runtime_error(vk::to_string(res));
    }
}