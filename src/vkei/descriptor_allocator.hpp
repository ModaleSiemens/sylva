#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    class DescriptorAllocator
    {
        public:
            struct PoolSizeRatio 
            {
                VkDescriptorType type;
                float ratio;
            };

            class DescriptorSetAllocationFailed : public std::runtime_error
            {
                public:
                    explicit DescriptorSetAllocationFailed(const VkResult result);

                    const VkResult result;
            };

            DescriptorAllocator() = default;

            DescriptorAllocator(const DescriptorAllocator&) = delete;
            DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

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

        private:   
            const std::size_t max_sets_per_pool {4092};
            const double sets_per_pool_grow_factor {1.5};

            std::vector<PoolSizeRatio> ratios;
            std::vector<VkDescriptorPool> full_pools;
            std::vector<VkDescriptorPool> ready_pools;

            std::uint32_t sets_per_pool;
    };
}