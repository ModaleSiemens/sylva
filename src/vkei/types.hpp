#pragma once

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include "descriptor_allocator.hpp"
#include "vk_mem_alloc.h"
#include "resource_cleaner.hpp"

namespace mdsm::vkei
{
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
    
        ResourceCleaner resource_cleaner;
    
        DescriptorAllocator frame_descriptors;
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
}