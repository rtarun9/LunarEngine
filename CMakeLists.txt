cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)

project(LunarEngine VERSION 0.1
    DESCRIPTION "Simple C++/DirectX11 Graphics Renderer"
    LANGUAGES CXX)

# Specify output paths for all configurations.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug-Bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug-Bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug-Bin)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release-Bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release-Bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release-Bin)

file(GLOB_RECURSE SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/**/*.cpp
    
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/**/*.hpp
)

add_subdirectory(external)

add_executable(LunarEngine ${SOURCE_FILES})

target_precompile_headers(LunarEngine PRIVATE include/Pch.hpp)
target_link_libraries(LunarEngine PRIVATE ThirdParty)
target_link_libraries(LunarEngine PRIVATE d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib)

set_property(TARGET LunarEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")