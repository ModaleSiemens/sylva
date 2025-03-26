#pragma once

#include <deque>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    class DescriptorWriter
    {
        public:
            DescriptorWriter() = default;

            DescriptorWriter(const DescriptorWriter&) = delete;
            DescriptorWriter& operator=(const DescriptorWriter&) = delete;

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

            void updateDescriptorSet(
                const VkDevice device, const VkDescriptorSet set
            );

            void clear();

        private:
            std::deque<VkDescriptorImageInfo> image_infos;
            std::deque<VkDescriptorBufferInfo> buffer_infos;
            std::vector<VkWriteDescriptorSet> writes;
    };
}