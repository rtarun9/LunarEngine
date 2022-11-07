include(FetchContent)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG 4c704ecfdd814f3ab4613b374f4b70ffed997662
)

FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG f46d329586b7bba140dc59d27e42497419d9e946 
)

FetchContent_Declare(
    DirectXMath
    GIT_REPOSITORY https://github.com/microsoft/DirectXMath
    GIT_TAG f639e2d8f54dd5431fac41c6f577113b1828d6f9
)

option(TINYGLTF_BUILD_LOADER_EXAMPLE "" OFF)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        544969b7324cd6bba29f6203c7d78c7ea92dbab0
)

FetchContent_MakeAvailable(SDL2 vk_bootstrap DirectXMath tinygltf)

add_library(ThirdParty INTERFACE)
target_link_libraries(ThirdParty INTERFACE SDL2 vk-bootstrap::vk-bootstrap DirectXMath tinygltf)

target_include_directories(ThirdParty INTERFACE {sdl2_SOURCE_DIRS}/include/ VulkanMemoryAllocator/)