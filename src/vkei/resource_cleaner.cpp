#include "resource_cleaner.hpp"
#include <SDL3/SDL_render.h>

namespace mdsm::vkei
{
    ResourceCleaner::~ResourceCleaner()
    {
        flush();
    }

    void ResourceCleaner::addCleaner(const std::function<void()>&& cleaner)
    {
        cleaners.emplace_back(
            std::move(cleaner)
        );
    }

    void ResourceCleaner::flush()
    {
        for(auto cleaner_it {cleaners.rbegin()}; cleaner_it != cleaners.rend(); ++cleaner_it)
        {
            (*cleaner_it)();
        }
    }
}