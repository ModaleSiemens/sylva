#pragma once

#include "metallic_roughness.hpp"
#include "node.hpp"
#include "resource_cleaner.hpp"
#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "types.hpp"

namespace mdsm::vkei
{
    class Engine
    {
        public:
            friend class MetallicRoughness;

            Engine(
                const std::string_view app_name,
                const std::size_t window_width,
                const std::size_t window_height,
                const std::string_view window_title = "",
                const bool debug = false
            );

            void draw();

            bool resizeRequested();
            
            void resizeSwapchain();            

            ~Engine();

            Engine(const Engine&) = delete;
            Engine& operator=(const Engine&) = delete;

        private:
            const bool debug;

            static constexpr std::size_t frame_overlap {2};

            bool stop_rendering {};
            bool resize_requested {};

            std::size_t frame_number {};

            ResourceCleaner resource_cleaner;

            vkb::Instance vkb_instance;
            vkb::PhysicalDevice vkb_physical_device;
            vkb::Device vkb_device;

            VkExtent2D window_extent;

            SDL_Window* window {};

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
    
            VkCommandBuffer immediate_command_buffer;
            VkCommandPool immediate_command_pool;
    
            VkFence immediate_fence;
    
            DescriptorAllocator global_descriptor_allocator;
    
            VkDescriptorSet draw_image_descriptors;
            VkDescriptorSetLayout draw_image_descriptor_layout;
    
            SceneData scene_data;
    
            VkDescriptorSetLayout scene_data_descriptor_layout;

            AllocatedImage default_texture;
            
            VkSampler default_linear_sampler;
            VkSampler default_nearest_sampler;

            MaterialInstance default_data;
            MetallicRoughness metal_rough_material;

            DrawContext main_draw_context;

            std::vector<std::shared_ptr<MeshAsset>> test_meshes;

            std::unordered_map<std::string_view, std::shared_ptr<Node>> loaded_nodes;

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
            void initializePipelines();
            void initializeDefaultData();
    
            void destroySwapchain();

            void updateScene();
    
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
}
