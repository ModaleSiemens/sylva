#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

struct DescriptorLayoutBuilder 
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(const std::uint32_t binding, const VkDescriptorType type);
    
    void clear();

    VkDescriptorSetLayout build(
        const VkDevice device,
        const VkShaderStageFlags shader_stages,
        const void* const p_next = nullptr,
        VkDescriptorSetLayoutCreateFlags flags = {}
    );
};