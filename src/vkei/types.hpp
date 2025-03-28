#pragma once

#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include "descriptor_allocator.hpp"
#include "vk_mem_alloc.h"
#include "resource_cleaner.hpp"

namespace mdsm::vkei
{
    class Engine;

    class VulkanException : public std::runtime_error
    {
        public: 
            explicit VulkanException(const VkResult error)
            :
                runtime_error {
                    std::format(
                        "A Vulkan error with code {} occourred!", string_VkResult(error)
                    )
                },
                error {error}
            {
            }

            const VkResult error;
    };
    
    enum class MaterialPass : std::uint8_t
    {
        Transparent,
        Opaque,
        MainColor
    };

    struct Vertex 
    {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;

        glm::vec4 color;
    };

    struct MaterialPipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    struct MaterialInstance
    {
        MaterialPipeline* pipeline;
        
        VkDescriptorSet descriptor_set;

        MaterialPass pass_type;
    };

    struct Material
    {
        MaterialInstance data;
    };

    struct RenderObject
    {
        std::uint32_t index_count;
        std::uint32_t first_index;

        VkBuffer index_buffer;

        MaterialInstance* material;

        glm::mat4 transform;

        VkDeviceAddress vertex_buffer_address;
    };

    struct DrawContext
    {
        std::vector<RenderObject> opaque_surfaces;
    };
 
    struct SceneData
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 view_proj;
        
        glm::vec4 ambient_color;
        glm::vec4 sunlight_direction;
        glm::vec4 sunlight_color;
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
    
    struct MeshBuffers
    {
        AllocatedBuffer index_buffer;
        AllocatedBuffer vertex_buffer;
        VkDeviceAddress vertex_buffer_address;
    };
    
    struct DrawPushCostants 
    {
        glm::mat4 world_matrix;
        VkDeviceAddress vertex_buffer;
    };

    struct Surface 
    {
        std::uint32_t start_index;
        std::uint32_t count;

        std::shared_ptr<Material> material;
    };

    struct MeshAsset
    {
        std::string name;

        std::vector<Surface> surfaces;
       
        MeshBuffers mesh_buffers;
    };    
}