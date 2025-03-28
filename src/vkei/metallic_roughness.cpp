#include "metallic_roughness.hpp"
#include "descriptor_layout_builder.hpp"
#include "types.hpp"
#include <format>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include "pipeline_builder.hpp"

#include "engine.hpp"

namespace mdsm::vkei
{
    MetallicRoughness::PipelineCreationFailed::PipelineCreationFailed(const VkResult error)
    :
        runtime_error {
            std::format(
                "Pipeline creation failed with error {}!", string_VkResult(error)
            )
        },
        error {error}
    {   
    }

    void MetallicRoughness::clearResources(const VkDevice device)
    {
        vkDestroyPipelineLayout(
            device, 
            opaque_pipeline.layout,
            nullptr
        );

        vkDestroyDescriptorSetLayout(
            device, 
            material_layout, 
            nullptr
        );

        vkDestroyPipeline(
            device, 
            opaque_pipeline.pipeline, 
            nullptr
        );

        vkDestroyPipeline(
            device, 
            transparent_pipeline.pipeline, 
            nullptr
        );        
    }

    MetallicRoughness::MetallicRoughness()
    :
        fragment_shader {VK_SHADER_STAGE_FRAGMENT_BIT},
        vertex_shader   {VK_SHADER_STAGE_VERTEX_BIT}
    {
    }

    void MetallicRoughness::buildPipeline(
        Engine* engine, 
        const std::string_view vertex_shader_path,
        const std::string_view fragment_shader_path       
    )
    {
        vertex_shader.setPath(vertex_shader_path);
        fragment_shader.setPath(fragment_shader_path);

        vertex_shader.compile(engine->logical_device);
        fragment_shader.compile(engine->logical_device);

        VkPushConstantRange matrix_range {};

        matrix_range.offset = 0;
        matrix_range.size = sizeof(DrawPushCostants);
        matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        DescriptorLayoutBuilder layout_builder;

        layout_builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        layout_builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        layout_builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        material_layout = layout_builder.build(
            engine->logical_device, 
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        );

        VkDescriptorSetLayout layouts[] {
            engine->scene_data_descriptor_layout,
            material_layout
        };

        VkPipelineLayoutCreateInfo mesh_layout_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
        };

        mesh_layout_info.setLayoutCount = 2;
        mesh_layout_info.pSetLayouts = layouts;
        mesh_layout_info.pPushConstantRanges = &matrix_range;
        mesh_layout_info.pushConstantRangeCount = 1;

        VkPipelineLayout new_layout;

        if(
            const auto result {
                vkCreatePipelineLayout(
                    engine->logical_device, 
                    &mesh_layout_info, 
                    nullptr, 
                    &new_layout
                )
            };
            result != VK_SUCCESS
        )
        {
            throw PipelineCreationFailed{result};
        }

        opaque_pipeline.layout = new_layout;
        transparent_pipeline.layout = new_layout;

        PipelineBuilder pipeline_builder;

        pipeline_builder.setShaders(vertex_shader, fragment_shader);
        pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        pipeline_builder.setMultisamplingToNone();
        pipeline_builder.disableBlending();
        pipeline_builder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        pipeline_builder.setColorAttachmentFormat(engine->draw_image.image_format);
        pipeline_builder.setDepthFormat(engine->depth_image.image_format);

        pipeline_builder.pipeline_layout = new_layout;

        opaque_pipeline.pipeline = pipeline_builder.build(engine->logical_device);

        pipeline_builder.enableAddictiveBlending();
        pipeline_builder.enableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

        transparent_pipeline.pipeline = pipeline_builder.build(engine->logical_device);

        vertex_shader.destroy(engine->logical_device);
        fragment_shader.destroy(engine->logical_device);
    }

    MaterialInstance MetallicRoughness::writeMaterial(
        const VkDevice device,
        const MaterialPass pass,
        const MaterialResources& resources,
        DescriptorAllocator& descriptor_allocator
    )
    {
        MaterialInstance material_data;

        material_data.pass_type = pass;

        if(pass == MaterialPass::Transparent)
        {
            material_data.pipeline = &transparent_pipeline;
        }
        else
        {
            material_data.pipeline = &opaque_pipeline;
        }

        material_data.descriptor_set = descriptor_allocator.allocate(device, material_layout);

        writer.clear();

        writer.writeBuffer(
            0, 
            resources.data_buffer, 
            sizeof(MaterialConstants), 
            resources.data_buffer_offset, 
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        );

        writer.writeImage(
            1, 
            resources.color_image.image_view, 
            resources.color_sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );

        writer.writeImage(
            1, 
            resources.metal_roughness_image.image_view, 
            resources.metal_roughness_sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );

        writer.updateDescriptorSet(device, material_data.descriptor_set);

        return material_data;
    }
}