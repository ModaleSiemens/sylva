#include "utils.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace mdsm::vkei
{
    VkImageCreateInfo generateImageCreateInfo(
        const VkFormat format, const VkImageUsageFlags usage_flags, const VkExtent3D extent
    )
    {
        VkImageCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.imageType = VK_IMAGE_TYPE_2D;
    
        info.format = format;
        info.extent = extent;
    
        info.mipLevels = 1;
        info.arrayLayers = 1;
    
        info.samples = VK_SAMPLE_COUNT_1_BIT;
    
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        
        info.usage = usage_flags;
    
        return info;
    }
    
    VkImageViewCreateInfo generateImageViewCreateInfo(
        const VkFormat format, const VkImage image, const VkImageAspectFlags aspect_flags
    )
    {
        VkImageViewCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    
        info.image = image;
        info.format = format;
    
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        info.subresourceRange.aspectMask = aspect_flags;
    
        return info;
    }
    
    VkCommandPoolCreateInfo generateCommandPoolCreateInfo(const std::uint32_t queue_family_index, const VkCommandPoolCreateFlags flags)
    {
        VkCommandPoolCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.queueFamilyIndex = queue_family_index;
        info.flags = flags;
    
        return info;
    }
    
    VkCommandBufferAllocateInfo generateCommandBufferAllocateInfo(const VkCommandPool pool, const std::uint32_t count)
    {
        VkCommandBufferAllocateInfo info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr
        };
    
        info.commandPool = pool;
        info.commandBufferCount = count;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    
        return info;
    }
    
    VkFenceCreateInfo generateFenceCreateinfo(const VkFenceCreateFlags flags)
    {
        VkFenceCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.flags = flags;
    
        return info;
    }
    
    VkSemaphoreCreateInfo generateSemaphoreCreateInfo(const VkSemaphoreCreateFlags flags)
    {
        VkSemaphoreCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.flags = flags;
    
        return info;
    }
    
    VkCommandBufferBeginInfo generateCommandBufferBeginInfo(const VkCommandBufferUsageFlags flags)
    {
        VkCommandBufferBeginInfo info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr
        };
    
        info.pInheritanceInfo = nullptr;
        
        info.flags = flags;
    
        return info;
    }
    
    VkImageSubresourceRange generateImageSubresourceRange(const VkImageAspectFlags aspect_mask)
    {
        VkImageSubresourceRange sub_image {};
    
        sub_image.aspectMask = aspect_mask;
        sub_image.baseMipLevel = 0;
        sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
        sub_image.baseArrayLayer = 0;
        sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;
    
        return sub_image;
    }
    
    VkSemaphoreSubmitInfo generateSemaphoreSubmitInfo(const VkPipelineStageFlags2 stage_mask, const VkSemaphore semaphore)
    {
        VkSemaphoreSubmitInfo info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr
        };
    
        info.semaphore = semaphore;
        info.stageMask = stage_mask;
        info.deviceIndex = 0;
        info.value = 1;
    
        return info;
    }
    
    VkCommandBufferSubmitInfo generateCommandBufferSubmitInfo(const VkCommandBuffer command_buffer)
    {
        VkCommandBufferSubmitInfo info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr
        };
    
        info.commandBuffer = command_buffer;
        info.deviceMask = 0;
    
        return info;
    }
    
    VkSubmitInfo2 generateSubmitInfo(const VkCommandBufferSubmitInfo *command_buffer, const VkSemaphoreSubmitInfo *signal_semaphore_info, const VkSemaphoreSubmitInfo *wait_semaphore_info)
    {
        VkSubmitInfo2 info {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr
        };
    
        info.waitSemaphoreInfoCount = wait_semaphore_info == nullptr? 0 : 1;
        info.pWaitSemaphoreInfos = wait_semaphore_info;
    
        info.signalSemaphoreInfoCount = signal_semaphore_info == nullptr? 0 : 1;
        info.pSignalSemaphoreInfos = signal_semaphore_info;
    
        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = command_buffer;
    
        return info;
    }
    
    void changeImageLayout(const VkCommandBuffer command_buffer, const VkImage image, const VkImageLayout current_layout, const VkImageLayout new_layout)
    {
        VkImageMemoryBarrier2 image_barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr
        };
    
        image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    
        image_barrier.oldLayout = current_layout;
        image_barrier.newLayout = new_layout;
    
        VkImageAspectFlags aspect_mask {
            new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL? 
            VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
        };     
    
        image_barrier.subresourceRange = generateImageSubresourceRange(aspect_mask);
        image_barrier.image = image;
    
        VkDependencyInfo dependency_info {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr
        };
    
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &image_barrier;
    
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }
    
    void copyImage(const VkCommandBuffer command_buffer, const VkImage source, const VkImage destination, const VkExtent2D source_size, const VkExtent2D destination_size)
    {
        VkImageBlit2 blit_region {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr
        };
    
        blit_region.srcOffsets[1].x = source_size.width;
        blit_region.srcOffsets[1].y = source_size.height;
        blit_region.srcOffsets[1].z = 1;
    
        blit_region.dstOffsets[1].x = destination_size.width;
        blit_region.dstOffsets[1].y = destination_size.height;
        blit_region.dstOffsets[1].z = 1;
    
        blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.srcSubresource.baseArrayLayer = 0;
        blit_region.srcSubresource.layerCount = 1;
        blit_region.srcSubresource.mipLevel = 0;
    
        blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit_region.dstSubresource.baseArrayLayer = 0;
        blit_region.dstSubresource.layerCount = 1;
        blit_region.dstSubresource.mipLevel = 0;
    
        VkBlitImageInfo2 blit_info {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr
        };
        
        blit_info.dstImage = destination;
        blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blit_info.srcImage = source;
        blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blit_info.filter = VK_FILTER_LINEAR;
        blit_info.regionCount = 1;
        blit_info.pRegions = &blit_region;
    
        vkCmdBlitImage2(command_buffer, &blit_info);    
    }
    
    VkRenderingAttachmentInfo generateDepthAttachmentInfo(const VkImageView view, const VkImageLayout layout)
    {
        VkRenderingAttachmentInfo depth_attachment {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr
        };
    
        depth_attachment.imageView = view;
        depth_attachment.imageLayout = layout;
    
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.clearValue.depthStencil.depth = 0.f;
        
        return depth_attachment;
    }
    
    VkRenderingAttachmentInfo generateAttachmentInfo(const VkImageView view, const VkClearValue *const clear, const VkImageLayout layout)
    {
        VkRenderingAttachmentInfo color_attachment {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr
        };
    
        color_attachment.imageView = view;
        color_attachment.imageLayout = layout;
        color_attachment.loadOp = clear? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
        if(clear)
        {
            color_attachment.clearValue = *clear;
        }
    
        return color_attachment;
    }
    
    VkRenderingInfo generateRenderingInfo(const VkExtent2D render_extent, const VkRenderingAttachmentInfo *const color_attachment, const VkRenderingAttachmentInfo *const depth_attachment)
    {
        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr
        };
    
        info.renderArea = VkRect2D{
            VkOffset2D{0, 0},
            render_extent
        };
    
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = color_attachment;
        info.pDepthAttachment = depth_attachment;
        info.pStencilAttachment = nullptr;
    
        return info;
    }
    
    VkPipelineShaderStageCreateInfo generatePipelineShaderStageCreateInfo(const VkShaderStageFlagBits stage, const VkShaderModule shader_module, const std::string_view entry)
    {
        VkPipelineShaderStageCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.stage = stage;
        info.module = shader_module;
        info.pName = entry.data();
    
        return info;
    }
    
    VkPipelineLayoutCreateInfo generatePipelineLayoutCreateInfo()
    {
        VkPipelineLayoutCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr
        };
    
        info.flags = 0;
        info.setLayoutCount = 0;
        info.pSetLayouts = nullptr;
        info.pushConstantRangeCount = 0;
        info.pPushConstantRanges = nullptr;
    
        return info;
    }
    
    bool loadShaderModule(const std::string_view file_path, const VkDevice device, VkShaderModule *const output_shader_module)
    {
        std::ifstream file {file_path.data(), std::ios::binary};
    
        if(!file.is_open())
        {
            return false;
        }
    
        const auto file_size {std::filesystem::file_size(file_path)};
    
        std::vector<std::uint32_t> buffer (file_size / sizeof(std::uint32_t));
    
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    
        file.close();
    
        VkShaderModuleCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr
        };
    
        create_info.codeSize = buffer.size() * sizeof(std::uint32_t);
        create_info.pCode = buffer.data();
    
        VkShaderModule shader_module;
    
        if(
            vkCreateShaderModule(
                device, &create_info, nullptr, &shader_module
            )
            != VK_SUCCESS
        )
        {
            return false;
        }
    
        *output_shader_module = shader_module;
    
        return true;
    }
}