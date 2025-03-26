#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <glm/glm.hpp>
#include "vk_mem_alloc.h"
#include "deletion_queue.hpp"
#include "growable_descriptor_allocator.hpp"

struct SceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
    glm::mat4 ambient_color;
    glm::mat4 sunlight_direction;
    glm::mat4 sunlight_color;
};

struct AllocatedImage 
{
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};

struct FrameData
{
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;

    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;

    VkFence render_fence;

    DeletionQueue deletors;

    GrowableDescriptorAllocator frame_descriptors;
};

struct AllocatedBuffer 
{
    VkBuffer buffer;

    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

struct Vertex 
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct MeshBuffers
{
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
};

struct GPUDrawPushCostants 
{
    glm::mat4 world_matrix;
    VkDeviceAddress vertex_buffer;
};

void check(const VkResult result);

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