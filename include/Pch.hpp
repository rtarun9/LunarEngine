#pragma once

#ifdef _DEBUG
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
#include <DirectXMath.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl.h>

// Global alias for Microsoft::WRL::ComPtr<T> and DirectX (used by DirectXMath).
namespace WRL = Microsoft::WRL;
namespace Math = DirectX;

// Custom global headers.
#include "LunarEngine/Utils.hpp"