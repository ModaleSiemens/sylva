#pragma once

#include "vulkan_engine.hpp"

class Game
{
    public:
        Game(const Game&) = delete;
        Game& operator=(const Game&) = delete;

        Game(
            const std::string_view app_name,
            const std::size_t window_width,
            const std::size_t window_height
        );

        void run();

    private:
        VulkanEngine vulkan_engine;
};