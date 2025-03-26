#pragma once

#include <deque>
#include <functional>
#include <type_traits>
namespace mdsm::vkei 
{
    class ResourceCleaner
    {
        public:
            ResourceCleaner() = default;
            ~ResourceCleaner();

            ResourceCleaner(const ResourceCleaner&) = delete;
            ResourceCleaner& operator=(const ResourceCleaner&) = delete;

            void addCleaner(const std::function<void()>&& cleaner);

            void flush();

        private:
            std::deque<std::function<void()>> cleaners;
    };
}