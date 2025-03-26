#include "descriptor_allocator.hpp"
#include <format>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    DescriptorAllocator::DescriptorSetAllocationFailed::DescriptorSetAllocationFailed(const VkResult result)
    :
        runtime_error {
            std::format(
                "Failed to allocate descriptor set with error {}!",
                string_VkResult(result)
            )
        },
        result {result}
    {
    }

    VkDescriptorPool DescriptorAllocator::getPool(const VkDevice device)
    {
        VkDescriptorPool new_pool;
    
        if(ready_pools.size())
        {
            new_pool = ready_pools.back();
            ready_pools.pop_back();
        }
        else 
        {
            new_pool = createPool(device, sets_per_pool, ratios);
    
            sets_per_pool *= sets_per_pool_grow_factor;
    
            if(sets_per_pool > max_sets_per_pool)
            {
                sets_per_pool = max_sets_per_pool;
            }
        }
    
        return new_pool;
    }
    
    VkDescriptorPool DescriptorAllocator::createPool(
        const VkDevice device,
        std::uint32_t set_count,
        const std::span<PoolSizeRatio> pool_ratios
    )
    {
        std::vector<VkDescriptorPoolSize> pool_sizes;
    
        for(const auto ratio: pool_ratios)
        {
            pool_sizes.push_back(
                VkDescriptorPoolSize{
                    .type = ratio.type,
                    .descriptorCount = static_cast<std::uint32_t>(ratio.ratio * set_count)
                }
            );
        }
    
        VkDescriptorPoolCreateInfo pool_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
        };
    
        pool_info.flags = 0;
        pool_info.maxSets = set_count;
        pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
    
        VkDescriptorPool new_pool;
    
        vkCreateDescriptorPool(
            device, &pool_info, nullptr, &new_pool
        );
    
        return new_pool;
    }
    
    void DescriptorAllocator::initialize(
        const VkDevice device,
        const std::uint32_t max_sets,
        const std::span<PoolSizeRatio> pool_ratios
    )
    {
        ratios.clear();
    
        for(const auto ratio: pool_ratios)
        {
            ratios.push_back(ratio);
        }
    
        VkDescriptorPool new_pool {
            createPool(device, max_sets, pool_ratios)
        };
    
        sets_per_pool = max_sets * sets_per_pool_grow_factor;
    
        ready_pools.push_back(new_pool);
    }
    
    void DescriptorAllocator::clearPools(const VkDevice device)
    {
        for(const auto pool: ready_pools)
        {
            vkResetDescriptorPool(device, pool, 0);
        }
    
        for(const auto pool : full_pools)
        {
            vkResetDescriptorPool(device, pool, 0);
            ready_pools.push_back(pool);
        }
    
        full_pools.clear();
    }
    
    void DescriptorAllocator::destroyPools(const VkDevice device)
    {
        for(const auto pool : ready_pools)
        {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
    
        ready_pools.clear();
    
        for(const auto pool : full_pools)
        {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
    
        full_pools.clear();
    }
    
    
    VkDescriptorSet DescriptorAllocator::allocate(
        const VkDevice device,
        const VkDescriptorSetLayout layout,
        const void* const p_next
    )
    {
        VkDescriptorPool pool_to_use {getPool(device)};
    
        VkDescriptorSetAllocateInfo allocate_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr
        };
    
        allocate_info.descriptorPool = pool_to_use;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout;
    
        VkDescriptorSet descriptor_set;
    
        VkResult result {
            vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set)
        };
    
        if(result == VK_ERROR_OUT_OF_POOL_MEMORY || VK_ERROR_FRAGMENTED_POOL)
        {
            full_pools.push_back(pool_to_use);
            
            pool_to_use = getPool(device);
            allocate_info.descriptorPool = pool_to_use;
    
            if(
                const auto result {
                    vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set)
                };
                result != VK_SUCCESS
            )
            {
                throw DescriptorSetAllocationFailed{result};
            }
        }
    
        ready_pools.push_back(pool_to_use);
    
        return descriptor_set;
    }    
}