#include "descriptor_layout_builder.hpp"

#include "vulkan_utils.hpp"

void DescriptorLayoutBuilder::addBinding(const std::uint32_t binding, const VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding new_bind {};

    new_bind.binding = binding;
    new_bind.descriptorCount = 1;
    new_bind.descriptorType = type;

    bindings.push_back(new_bind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(
    const VkDevice device,
    const VkShaderStageFlags shader_stages,
    const void* const p_next,
    VkDescriptorSetLayoutCreateFlags flags
)
{
    for(auto& binding : bindings)
    {
        binding.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr
    };

    info.pBindings = bindings.data();
    info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    info.flags = flags;

    VkDescriptorSetLayout set;

    check(
        vkCreateDescriptorSetLayout(
            device, &info, nullptr, &set)
    );

    return set;
}