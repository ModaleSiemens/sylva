#include "descriptor_writer.hpp"

#include <cstdint>

void DescriptorWriter::writeBuffer(
    const int binding,
    const VkBuffer buffer,
    const std::size_t size,
    const std::size_t offset,
    const VkDescriptorType type
)
{
    VkDescriptorBufferInfo& info {
        buffer_infos.emplace_back(
            VkDescriptorBufferInfo{
                .buffer = buffer,
                .offset = offset,
                .range = size
            }
        )
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
    };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::writeImage(
    const int binding,
    const VkImageView image,
    const VkSampler sampler,
    const VkImageLayout layout,
    const VkDescriptorType type
)
{
    VkDescriptorImageInfo& info {
        image_infos.emplace_back(
            VkDescriptorImageInfo{
                .sampler = sampler,
                .imageView = image,
                .imageLayout = layout
            }
        )
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
    };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    image_infos.clear();
    writes.clear();
    buffer_infos.clear();
}

void DescriptorWriter::updateSet(const VkDevice device, const VkDescriptorSet set)
{
    for(auto& write : writes)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(
        device, 
        static_cast<std::uint32_t>(writes.size()), 
        writes.data(), 
        0, 
        nullptr
    );
}
