#pragma once

#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    class DescriptorLayoutBuilder 
    {
        class DescriptorSetLayoutCreationFailed : public std::runtime_error
        {
            public:
                explicit DescriptorSetLayoutCreationFailed(const VkResult result);

                const VkResult result;
        };

        public:
            DescriptorLayoutBuilder() = default;

            DescriptorLayoutBuilder(const DescriptorLayoutBuilder&) = delete;
            DescriptorLayoutBuilder& operator=(const DescriptorLayoutBuilder&) = delete;

            void addBinding(const std::uint32_t binding, const VkDescriptorType type);

            void clear();

            VkDescriptorSetLayout build(
                const VkDevice device,
                const VkShaderStageFlags shader_stages,
                const void* const p_next = nullptr,
                const VkDescriptorSetLayoutCreateFlags flags = {}
            );

        private:
            std::vector<VkDescriptorSetLayoutBinding> bindings;
    };
}
