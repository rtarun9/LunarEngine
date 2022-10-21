include(FetchContent)

FetchContent_Declare(
    CrossWindow
    GIT_REPOSITORY https://github.com/alaingalvan/CrossWindow
    GIT_TAG 3511e43d0bbdc5fd23b4717580cc47ad3df87e4c
)

FetchContent_MakeAvailable(CrossWindow)

add_library(ThirdParty INTERFACE)
target_link_libraries(ThirdParty INTERFACE CrossWindow)