#include "game.hpp"

#include <thread>
#include <chrono>

#include <SDL3/SDL.h>

Game::Game(
    const std::string_view app_name,
    const std::size_t window_width,
    const std::size_t window_height
)
:
    vulkan_engine {app_name, window_width, window_height, app_name, true}
{
}

void Game::run()
{
    using namespace std::chrono_literals;

    SDL_Event event;

    bool quit {};

    while(!quit)
    {
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }

            if(event.type == SDL_EVENT_WINDOW_MINIMIZED)
            {
                minimized = true;
            }

            if(event.type == SDL_EVENT_WINDOW_RESTORED)
            {
                minimized = false;
            }
        }

        if(minimized)
        {
            std::this_thread::sleep_for(100ms);
            continue;
        }

        if(vulkan_engine.resizeRequested())
        {
            vulkan_engine.resizeSwapchain();
        }

        vulkan_engine.draw();
    }
}
