#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <deque>

struct DescriptorWriter
{
    std::deque<VkDescriptorImageInfo> image_infos;
    std::deque<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(
        const int binding,
        const VkImageView image,
        const VkSampler sampler,
        const VkImageLayout layout,
        const VkDescriptorType type
    );

    void writeBuffer(
        const int binding,
        const VkBuffer buffer,
        const std::size_t size,
        const std::size_t offset,
        const VkDescriptorType type
    );

    void clear();

    void updateSet(const VkDevice device, const VkDescriptorSet set);
};