#pragma once

#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    class PipelineBuilder 
    {
        public:
            class PipelineCreationFailed : public std::runtime_error
            {
                public:
                    explicit PipelineCreationFailed(const VkResult result);

                    const VkResult result;
            };  

            PipelineBuilder();
            
            PipelineBuilder(const PipelineBuilder&) = delete;
            PipelineBuilder& operator=(const PipelineBuilder&) = delete;

            void clear();
    
            void setShaders(
                const VkShaderModule vertex_shader,
                const VkShaderModule fragment_shader
            );
    
            void setInputTopology(const VkPrimitiveTopology topology);
            void setPolygonMode(const VkPolygonMode mode);
            void setCullMode(const VkCullModeFlags cull_mode, const VkFrontFace front_face);
            void setMultisamplingToNone();
            void disableBlending();
    
            void setColorAttachmentFormat(const VkFormat format); 
            void setDepthFormat(const VkFormat format);
    
            void disableDepthTest();
            void enableDepthTest(const bool depth_write_enable, const VkCompareOp op);

            void enableAddictiveBlending();
            void enableAlphablendBlending();
    
            VkPipeline build(const VkDevice device);
    
        public:
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

            VkPipelineInputAssemblyStateCreateInfo input_assembly;
            VkPipelineRasterizationStateCreateInfo rasterizer;
            VkPipelineColorBlendAttachmentState color_blend_attachment;
            VkPipelineMultisampleStateCreateInfo multisampling;
            VkPipelineLayout pipeline_layout;
            VkPipelineDepthStencilStateCreateInfo depth_stencil;
            VkPipelineRenderingCreateInfo render_info;
            VkFormat color_attachment_format;
    };
}