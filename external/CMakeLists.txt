include(FetchContent)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG 4c704ecfdd814f3ab4613b374f4b70ffed997662
)

FetchContent_MakeAvailable(SDL2)

add_library(ThirdParty INTERFACE)
target_link_libraries(ThirdParty INTERFACE SDL2)

target_include_directories(ThirdParty INTERFACE {SDL2_SOURCE_DIRS}/sdl2-src/include)