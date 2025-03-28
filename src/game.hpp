#pragma once

#include "vkei/engine.hpp"

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
        mdsm::vkei::Engine vulkan_engine;

        bool minimized {};
};