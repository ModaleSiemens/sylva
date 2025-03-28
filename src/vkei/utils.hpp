#pragma once

#include <cstdint>
#include <string_view>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    VkImageCreateInfo generateImageCreateInfo(
        const VkFormat format, const VkImageUsageFlags usage_flags, const VkExtent3D extent
    );
    
    VkImageViewCreateInfo generateImageViewCreateInfo(
        const VkFormat format, const VkImage image, const VkImageAspectFlags aspect_flags
    );
    
    VkCommandPoolCreateInfo generateCommandPoolCreateInfo(
        const std::uint32_t queue_family_index, const VkCommandPoolCreateFlags flags = {}
    );
    
    VkCommandBufferAllocateInfo generateCommandBufferAllocateInfo(
        const VkCommandPool pool, const std::uint32_t count = 1
    );
    
    VkFenceCreateInfo generateFenceCreateinfo(const VkFenceCreateFlags flags = {});
    
    VkSemaphoreCreateInfo generateSemaphoreCreateInfo(const VkSemaphoreCreateFlags flags = {});
    
    VkCommandBufferBeginInfo generateCommandBufferBeginInfo(
        const VkCommandBufferUsageFlags flags = {}
    );
    
    VkImageSubresourceRange generateImageSubresourceRange(
        const VkImageAspectFlags aspect_mask
    );
    
    VkSemaphoreSubmitInfo generateSemaphoreSubmitInfo(
        const VkPipelineStageFlags2 stage_mask,
        const VkSemaphore semaphore
    );
    
    VkCommandBufferSubmitInfo generateCommandBufferSubmitInfo(
        const VkCommandBuffer command_buffer
    );
    
    VkSubmitInfo2 generateSubmitInfo(
        const VkCommandBufferSubmitInfo* command_buffer,
        const VkSemaphoreSubmitInfo* signal_semaphore_info,
        const VkSemaphoreSubmitInfo* wait_semaphore_info
    );
    
    void changeImageLayout(
        const VkCommandBuffer command_buffer,
        const VkImage image,
        const VkImageLayout current_layout,
        const VkImageLayout new_layout
    );
    
    void copyImage(
        const VkCommandBuffer command_buffer,
        const VkImage source,
        const VkImage destination,
        const VkExtent2D source_size,
        const VkExtent2D destination_size
    );
    
    VkPipelineShaderStageCreateInfo generatePipelineShaderStageCreateInfo(
        const VkShaderStageFlagBits stage,
        const VkShaderModule shader_module,
        const std::string_view entry = "main"
    );
    
    VkPipelineLayoutCreateInfo generatePipelineLayoutCreateInfo();
    
    bool loadShaderModule(
        const std::string_view file_path,
        const VkDevice device,
        VkShaderModule* const output_shader_module
    );
    
    VkRenderingAttachmentInfo generateAttachmentInfo(
        const VkImageView view,
        const VkClearValue* const clear,
        const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
    
    VkRenderingInfo generateRenderingInfo(
        const VkExtent2D render_extent,
        const VkRenderingAttachmentInfo* const color_attachment,
        const VkRenderingAttachmentInfo* const depth_attachment
    );
    
    VkRenderingAttachmentInfo generateDepthAttachmentInfo(
        const VkImageView view, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    void check(const VkResult result);
}