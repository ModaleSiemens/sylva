#pragma once

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include "vk_mem_alloc.h"

#include <string_view>
#include <cstdint>
#include <deque>
#include <functional>
#include <array>
#include <span>

#include <glm/glm.hpp>

namespace vulkan_utils 
{
    struct DeletionQueue
    {
        std::deque<std::function<void()>> deletors;

        void addDeletor(std::function<void()>&& deletor);

        void flush();
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

    struct GPUMeshBuffers
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

    class PipelineBuilder 
    {
        public:
            PipelineBuilder();
            
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

            VkPipeline build(const VkDevice device);
    
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
}

class VulkanEngine
{
    public:
        VulkanEngine(
            const std::string_view app_name,
            const std::size_t window_width,
            const std::size_t window_height,
            const std::string_view window_title
        );

        void draw();

        bool resizeRequested();
        
        void resizeSwapchain();

        ~VulkanEngine();

    private:
        const bool use_validation_layers {true};

        static constexpr std::size_t frame_overlap {2};

        bool stop_rendering {false};
        bool resize_requested {false};

        std::size_t frame_number {};

        vulkan_utils::DeletionQueue deletors;

        vkb::Instance vkb_instance;
        vkb::PhysicalDevice vkb_physical_device;
        vkb::Device vkb_logical_device;

        VkExtent2D window_extent;

        SDL_Window* window {nullptr};

        VkInstance instance;
        VkDebugUtilsMessengerEXT debug_messenger;

        VkSurfaceKHR surface;

        VkPhysicalDevice physical_device;
        VkDevice         logical_device;

        VkSwapchainKHR swapchain;
        VkFormat swapchain_image_format;

        VkExtent2D swapchain_extent;

        std::vector<VkImage>     swapchain_images;
        std::vector<VkImageView> swapchain_image_views;         

        vulkan_utils::AllocatedImage draw_image;
        VkExtent2D draw_extent;

        float render_scale {1.f};

        std::array<vulkan_utils::FrameData, frame_overlap> frames;

        VkQueue graphics_queue;

        std::uint32_t graphics_queue_family;
        
        VmaAllocator allocator;

        VkPipelineLayout triangle_pipeline_layout;
        VkPipeline triangle_pipeline;

        VkPipelineLayout mesh_pipeline_layout;
        VkPipeline mesh_pipeline;

        vulkan_utils::GPUMeshBuffers rectangle;

        VkCommandBuffer immediate_command_buffer;
        VkCommandPool immediate_command_pool;

        VkFence immediate_fence;

        void initializeWindow(const std::size_t width,
            const std::size_t height,
            const std::string_view title
        );
        void initializeInstance(const std::string_view app_name);
        void initializePhysicalDevice();
        void initializeLogicalDevice();
        void initializeAllocator();
        void createSwapchain(const std::size_t width, const std::size_t height);
        void initializeSwapchain();
        void initializeQueues();
        void initializeCommands();
        void initializeSyncStructures();

        void initializeTrianglePipeline();
        void initializeMeshPipeline();

        void initializeDefaultData();

        void destroySwapchain();

        vulkan_utils::AllocatedBuffer createBuffer(
            const std::size_t allocate_size,
            const VkBufferUsageFlags usage,
            const VmaMemoryUsage memory_usage
        );

        void destroyBuffer(const vulkan_utils::AllocatedBuffer& buffer);

        void immediateSubmit(
            const std::function<void(const VkCommandBuffer command_buffer)>&& function
        );

        vulkan_utils::GPUMeshBuffers uploadMesh(
            const std::span<std::uint32_t> indices,
            const std::span<vulkan_utils::Vertex> vertices
        );

        void drawBackground(const VkCommandBuffer command_buffer);
        void drawGeometry(const VkCommandBuffer command_buffer);

        void cleanup();

        vulkan_utils::FrameData& getCurrentFrame();

        void check(const VkResult result);
};