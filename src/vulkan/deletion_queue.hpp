#pragma once

#include <functional>
#include <deque>

struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void addDeletor(std::function<void()>&& deletor);

    void flush();
};