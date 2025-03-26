#include "pipeline_builder.hpp"

#include <stdexcept>
#include "vulkan_utils.hpp"

PipelineBuilder::PipelineBuilder()
{
    clear();
}

void PipelineBuilder::clear()
{
    input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };

    color_blend_attachment = {};

    multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };

    pipeline_layout = {};

    depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    render_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO
    };

    shader_stages.clear();
}

void PipelineBuilder::setShaders(const VkShaderModule vertex_shader, const VkShaderModule fragment_shader)
{
    shader_stages.clear();

    shader_stages.push_back(
        generatePipelineShaderStageCreateInfo(
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_shader
        )
    );

    shader_stages.push_back(
        generatePipelineShaderStageCreateInfo(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader
        )
    );    
}

void PipelineBuilder::setInputTopology(const VkPrimitiveTopology topology)
{
    input_assembly.topology = topology;

    input_assembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setPolygonMode(const VkPolygonMode mode)
{
    rasterizer.polygonMode = mode;
    rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::setCullMode(const VkCullModeFlags cull_mode, const VkFrontFace front_face)
{
    rasterizer.cullMode = cull_mode;
    rasterizer.frontFace = front_face;
}

void PipelineBuilder::setMultisamplingToNone()
{
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disableBlending()
{
    color_blend_attachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_A_BIT   
    ;
    
    color_blend_attachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachmentFormat(const VkFormat format)
{
    color_attachment_format = format;

    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachmentFormats = &color_attachment_format;
}

void PipelineBuilder::setDepthFormat(const VkFormat format)
{
    render_info.depthAttachmentFormat = format;
}

void PipelineBuilder::disableDepthTest()
{
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
}

VkPipeline PipelineBuilder::build(const VkDevice device)
{
    VkPipelineViewportStateCreateInfo viewport_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr
    };

    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blending {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr
    };

    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineVertexInputStateCreateInfo vertex_input_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 
    };

    VkGraphicsPipelineCreateInfo pipeline_info {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };

    pipeline_info.pNext = &render_info;

    pipeline_info.stageCount = static_cast<std::uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = pipeline_layout;

    VkDynamicState state[] {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };

    dynamic_info.pDynamicStates = state;
    dynamic_info.dynamicStateCount = 2;

    pipeline_info.pDynamicState = &dynamic_info;

    VkPipeline new_pipeline;

    if(
        vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &new_pipeline
        ) 
        != VK_SUCCESS
    )
    {
        throw std::runtime_error{"Failed to create pipeline!"};
    }
    
    return new_pipeline;
}