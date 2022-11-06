#pragma once

#ifdef DEF_LUNAR_DEBUG
constexpr bool LUNAR_DEBUG = true;
#else
constexpr bool LUNAR_DEBUG = false;
#endif

// STL includes.
#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <exception>
#include <iostream>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <source_location>
#include <format>
#include <fstream>

// Vulkan includes.
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

// Math includes.
#include <DirectXMath.h>

// Custom global headers.
#include "LunarEngine/PrimitiveTypes.hpp"
#include "LunarEngine/Utils.hpp"

// Global aliases
namespace math = DirectX;