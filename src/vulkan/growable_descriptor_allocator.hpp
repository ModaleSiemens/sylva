#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <span>

struct GrowableDescriptorAllocator
{
    const std::size_t max_sets_per_pool {4092};
    const double sets_per_pool_grow_factor {1.5};

    struct PoolSizeRatio 
    {
        VkDescriptorType type;
        float ratio;
    };

    void initialize(
        const VkDevice device,
        const std::uint32_t initial_sets,
        const std::span<PoolSizeRatio> pool_ratios
    );

    void clearPools(const VkDevice device);
    void destroyPools(const VkDevice device);

    VkDescriptorSet allocate(
        const VkDevice device,
        const VkDescriptorSetLayout layout,
        const void* const p_next = nullptr
    );

    VkDescriptorPool getPool(const VkDevice device);

    VkDescriptorPool createPool(
        const VkDevice device,
        const std::uint32_t set_count,
        const std::span<PoolSizeRatio> pool_ratios
    );

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> full_pools;
    std::vector<VkDescriptorPool> ready_pools;

    std::uint32_t sets_per_pool;
};