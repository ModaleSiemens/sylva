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

#include <glm/vec2.hpp>

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
};

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

        ~VulkanEngine();

    private:
        std::deque<std::function<void()>> deletors;
        
        const bool use_validation_layers {true};

        static constexpr std::size_t frame_overlap {2};

        bool stop_rendering {false};

        std::size_t frame_number {};

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
        VkExtent2D draw_extent;

        std::array<FrameData, frame_overlap> frames;

        VkQueue graphics_queue;

        std::uint32_t graphics_queue_family;
        
        VmaAllocator allocator;

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

        void cleanup();

        FrameData& getCurrentFrame();

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

        void check(const VkResult result);
};