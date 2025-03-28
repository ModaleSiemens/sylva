#pragma once

#include "descriptor_allocator.hpp"
#include "types.hpp"
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vulkan/vulkan_core.h>
#include "descriptor_writer.hpp"
#include "shader.hpp"

namespace mdsm::vkei
{
    struct MetallicRoughness
    {
        class PipelineCreationFailed : public std::runtime_error
        {
            public:
                explicit PipelineCreationFailed(const VkResult error);

                const VkResult error;
        };

        MetallicRoughness();

        MaterialPipeline opaque_pipeline;
        MaterialPipeline transparent_pipeline;

        VkDescriptorSetLayout material_layout;

        struct MaterialConstants
        {
            glm::vec4 color_factors;
            glm::vec4 metal_roughness_factor;

            // padding
            glm::vec4 extra[14];
        };

        struct MaterialResources
        {
            AllocatedImage color_image;
            
            VkSampler color_sampler;    
            
            AllocatedImage metal_roughness_image;
            VkSampler metal_roughness_sampler;

            VkBuffer data_buffer;

            std::uint32_t data_buffer_offset;
        };

        DescriptorWriter writer;

        void buildPipeline(
            Engine* engine,
            const std::string_view vertex_shader_path,
            const std::string_view fragment_shader_path
        );
        
        void clearResources(const VkDevice device);

        MaterialInstance writeMaterial(
            const VkDevice device,
            const MaterialPass pass,
            const MaterialResources& resources,
            DescriptorAllocator& descriptor_allocator
        );

        Shader vertex_shader;
        Shader fragment_shader;
    };
}