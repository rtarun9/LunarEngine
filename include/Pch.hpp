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

// DirectX / DXGI includes.
#include <d3d12.h>
#include <DirectXMath.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>

// Global alias for Microsoft::WRL::ComPtr<T> and DirectX (used by DirectXMath).
namespace WRL = Microsoft::WRL;
namespace Math = DirectX;

// Custom global headers.
#include "LunarEngine/d3dx12.hpp"

#include "LunarEngine/PrimitiveTypes.hpp"
#include "LunarEngine/Utils.hpp"