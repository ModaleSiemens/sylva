#include "game.hpp"

#include <print>

int main()
{
    Game game {"Sylva!", 800, 600};

    std::println("Hello, Sylva!");

    game.run();

    return 0;
}