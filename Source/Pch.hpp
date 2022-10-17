#pragma once

#ifdef DEF_LUNAR_DEBUG
    constexpr bool LUNAR_DEBUG = true;
#else
    constexpr bool LUNAR_DEBUG = false;
#endif
   
// STL includes.
#include <iostream>
#include <array>
#include <vector>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <chrono>
#include <ranges>
#include <exception>
#include <stdexcept>

// DirectX / DXGI includes.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

// Common includes.
#include "Common/DataTypes.hpp"
#include "Common/Helpers.hpp"
#include "Common/Globals.hpp"

// For setting the Agility SDK parameters.
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 602u;
}

extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}