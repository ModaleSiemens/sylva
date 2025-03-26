#include "deletion_queue.hpp"

#include <print>

void DeletionQueue::addDeletor(std::function<void()> &&deletor)
{
    deletors.emplace_back(std::move(deletor));
}

void DeletionQueue::flush()
{
    for(auto deletor_it {deletors.rbegin()}; deletor_it != deletors.rend(); ++deletor_it)
    {
        (*deletor_it)();
    }

    deletors.clear();
}