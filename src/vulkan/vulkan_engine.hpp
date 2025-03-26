#pragma once

//#define VMA_DEBUG_LOG

#include "vk_mem_alloc.h"

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include "deletion_queue.hpp"
#include "growable_descriptor_allocator.hpp"

#include <string_view>
#include <cstdint>
#include <functional>
#include <span>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "vulkan_utils.hpp"

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

        DeletionQueue deletors;

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

        AllocatedImage draw_image;
        AllocatedImage depth_image;

        VkExtent2D draw_extent;

        float render_scale {1.f};

        std::array<FrameData, frame_overlap> frames;

        VkQueue graphics_queue;

        std::uint32_t graphics_queue_family;
        
        VmaAllocator allocator;

        VkPipelineLayout triangle_pipeline_layout;
        VkPipeline triangle_pipeline;

        VkPipelineLayout mesh_pipeline_layout;
        VkPipeline mesh_pipeline;

        MeshBuffers rectangle;

        VkCommandBuffer immediate_command_buffer;
        VkCommandPool immediate_command_pool;

        VkFence immediate_fence;

        GrowableDescriptorAllocator global_descriptor_allocator;

        VkDescriptorSet draw_image_descriptors;
        VkDescriptorSetLayout draw_image_descriptor_layout;

        SceneData scene_data;

        VkDescriptorSetLayout scene_data_descriptor_layout;

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
        void initializeDescriptors();

        void initializeTrianglePipeline();
        void initializeMeshPipeline();

        void initializeDefaultData();

        void destroySwapchain();

        AllocatedBuffer createBuffer(
            const std::size_t allocate_size,
            const VkBufferUsageFlags usage,
            const VmaMemoryUsage memory_usage
        );

        void destroyBuffer(const AllocatedBuffer& buffer);

        void immediateSubmit(
            const std::function<void(const VkCommandBuffer command_buffer)>&& function
        );

        MeshBuffers uploadMesh(
            const std::span<std::uint32_t> indices,
            const std::span<Vertex> vertices
        );

        AllocatedImage createImage(
            const VkExtent3D size,
            const VkFormat format,
            const VkImageUsageFlags usage,
            bool mipmapped = false 
        );

        AllocatedImage createImage(
            const void* const data,
            const VkExtent3D size,
            const VkFormat format,
            const VkImageUsageFlags usage,
            bool mipmapped = false             
        );

        void destroyImage(const AllocatedImage& image);

        void drawBackground(const VkCommandBuffer command_buffer);
        void drawGeometry(const VkCommandBuffer command_buffer);

        void cleanup();

        FrameData& getCurrentFrame();
};