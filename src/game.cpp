#include "game.hpp"

Game::Game(
    const std::string_view app_name,
    const std::size_t window_width,
    const std::size_t window_height
)
:
    vulkan_engine {app_name, window_width, window_height, app_name}
{
}

void Game::run()
{
    while(true)
    {
        vulkan_engine.draw();
    }
}
