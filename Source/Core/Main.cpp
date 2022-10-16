#include "Pch.hpp"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "Engine.hpp"

int main() 
{
    try
    {
        // Initialize SDL2 and create window.
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            FatalError(L"Failed to initialize SDL2.");
        }

        // Set DPI awareness on Windows.
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

        const Config config
        {
            .window_width = 1080u,
            .window_height = 720u,
            .window_title = "Lunar Engine"
        };

        const SDL_Window* window = SDL_CreateWindow(config.window_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.window_width, config.window_height, SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window)
        {
            FatalError(L"Failed to create SDL2 window.");
        }

        // Setup engine.
        lunar::core::Engine engine{ config };

        uint32_t previous_frame_time = 0u;
        
        // Main game loop.
        bool quit = false;

        while (!quit)
        {
            const uint32_t current_frame_time = SDL_GetTicks();
            const float delta_time = static_cast<float>((current_frame_time - previous_frame_time) * 1e-3);
            previous_frame_time = current_frame_time;

            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }

                const uint8_t* current_key_state = SDL_GetKeyboardState(nullptr);
                if (current_key_state[SDL_SCANCODE_ESCAPE])
                {
                    quit = true;
                }
            }

            engine.Update(delta_time);
            engine.Render();
        }
    }
    catch (const std::exception& exception)
    {
        std::cout << "[Exception Caught] : " << exception.what() << '\n';
        return -1;
    }

    return 0;
}