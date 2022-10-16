workspace "LunarEngine"
    configurations { "Debug", "Release" }
    architecture "x86_64"
    system "windows"

project "LunarEngine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    warnings "High"

    dpiawareness "High"

    targetdir "Bin/%{cfg.buildcfg}"
    objdir "BinInt/%{cfg.buildcfg}"
    
    includedirs { "Source/" }

    pchheader "Pch.hpp"
    pchsource "Source/Pch.cpp"
    
    files { "Source/**.hpp", "Source/**.cpp" }

    filter "configurations:Debug"
        defines { "DEF_LUNAR_DEBUG" }
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        optimize "On"
        optimize "Speed"