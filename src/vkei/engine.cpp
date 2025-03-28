#include "descriptor_layout_builder.hpp"
#include "descriptor_writer.hpp"
#include "mesh_node.hpp"
#include "metallic_roughness.hpp"
#include "pipeline_builder.hpp"
#include "types.hpp"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/fwd.hpp>
#include <glm/packing.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <ostream>
#include <vk_video/vulkan_video_codec_av1std.h>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION

#include <glm/gtc/matrix_transform.hpp>

#include "vk_mem_alloc.h"

#include "engine.hpp"

#include <VkBootstrap.h>

#include "utils.hpp"

#include <print>

namespace mdsm::vkei
{
    Engine::Engine(
        const std::string_view app_name,
        const std::size_t window_width,
        const std::size_t window_height,
        const std::string_view window_title,
        const bool debug
    )
    :
        debug {debug},
        metal_rough_material {}
    {
        initializeWindow(window_width, window_height, window_title);
        initializeInstance(app_name);
        initializePhysicalDevice();
        initializeLogicalDevice();
        initializeAllocator();
        initializeSwapchain();
        initializeQueues();
        initializeCommands();
        initializeSyncStructures();
        initializeDescriptors();
        initializePipelines();
        initializeDefaultData();
    }
    
    void Engine::initializeDefaultData()
    {
        const std::uint32_t black {
            glm::packUnorm4x8(
                glm::vec4{0, 0, 0, 0}
            )
        };

        const std::uint32_t magenta {
            glm::packUnorm4x8(
                glm::vec4{1, 0, 1, 1}
            )
        };

        std::array<std::uint32_t, 32 * 32> pixels;

        for(std::size_t x {}; x < 32; ++x)
        {
            for(std::size_t y {}; y < 32; ++y)
            {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2))? magenta : black;
            }
        }

        default_texture = createImage(
            pixels.data(),
            VkExtent3D{32, 32, 1},
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT
        );

        VkSamplerCreateInfo sampler {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO 
        };

        sampler.magFilter = VK_FILTER_NEAREST;
        sampler.minFilter = VK_FILTER_NEAREST;

        vkCreateSampler(
            logical_device, 
            &sampler, 
            nullptr, 
            &default_nearest_sampler    
        );

        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;

        vkCreateSampler(
            logical_device, 
            &sampler, 
            nullptr, 
            &default_linear_sampler    
        );        

        resource_cleaner.addCleaner(
            [&, this]
            {
                vkDestroySampler(logical_device, default_nearest_sampler, nullptr);
                vkDestroySampler(logical_device, default_linear_sampler, nullptr);

                destroyImage(default_texture);
            }
        );

        MetallicRoughness::MaterialResources material_resources;

        material_resources.color_image = default_texture;
        material_resources.color_sampler = default_linear_sampler;
        
        material_resources.metal_roughness_image = default_texture;
        material_resources.metal_roughness_sampler = default_linear_sampler;

        AllocatedBuffer material_constants {
            createBuffer(
                sizeof(MetallicRoughness::MaterialConstants), 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VMA_MEMORY_USAGE_CPU_TO_GPU
            )
        };

        MetallicRoughness::MaterialConstants* scene_uniform_data {
            reinterpret_cast<MetallicRoughness::MaterialConstants*>(
                material_constants.allocation->GetMappedData()
            )
        };

        scene_uniform_data->color_factors = glm::vec4{1, 1, 1, 1};
        scene_uniform_data->metal_roughness_factor = glm::vec4{1, 0.5, 0, 0};

        resource_cleaner.addCleaner(
            [=, this]
            {
                if(debug) std::println("Destroying material buffer");

                destroyBuffer(material_constants);
            }
        );

        material_resources.data_buffer = material_constants.buffer;
        material_resources.data_buffer_offset = 0;

        default_data = metal_rough_material.writeMaterial(
            logical_device, 
            MaterialPass::MainColor, 
            material_resources,
            global_descriptor_allocator
        );

        /*

        MeshAsset cube;

        cube.name = "cube";

        std::shared_ptr<Material> cube_material {
            std::make_shared<Material>(
                default_data
            )
        };

        cube.surfaces.push_back(
            {
                .start_index = 0,
                .count = 8,
                .material = cube_material
            }
        );

        std::vector<Vertex> cube_vertices {
            // Front face
            {{-0.5f, -0.5f,  0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},
        
            // Back face
            {{-0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {0.0f, 1.0f, 1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {0.5f, 0.5f, 0.5f, 1.0f}},
        };        

        std::vector<std::uint32_t> cube_indices {
            0, 1, 2, 2, 3, 0, // Front face
            1, 5, 6, 6, 2, 1, // Right face
            5, 4, 7, 7, 6, 5, // Back face
            4, 0, 3, 3, 7, 4, // Left face
            3, 2, 6, 6, 7, 3, // Top face
            4, 5, 1, 1, 0, 4  // Bottom face
        };        

        cube.mesh_buffers = uploadMesh(
            cube_indices, cube_vertices
        );

        const auto cube_asset {std::make_shared<MeshAsset>(cube)};

        auto cube_mesh {std::make_shared<MeshNode>()};

        cube_mesh->mesh = cube_asset;

        loaded_nodes["cube"] = cube_mesh;

    */

        for(auto& mesh : test_meshes)
        {
            std::shared_ptr<MeshNode> new_node {
                std::make_shared<MeshNode>()
            };

            new_node->mesh = mesh;

            new_node->local_transform = glm::mat4{1.f};
            new_node->world_transform = glm::mat4{1.f};

            for(auto& surface : new_node->mesh->surfaces)
            {
                surface.material = std::make_shared<Material>(default_data);
            }

            loaded_nodes[mesh->name] = std::move(new_node);
        }
    }

    void Engine::initializePipelines()
    {
        metal_rough_material.buildPipeline(
            this,
            "../shaders/mesh.vert.spv",
            "../shaders/mesh.frag.spv"
        );
    }

    void Engine::updateScene()
    {
        main_draw_context.opaque_surfaces.clear();

        //loaded_nodes["cube"]->draw(glm::mat4{1.f}, main_draw_context);

        scene_data.view = glm::translate(glm::mat4{1.0f}, glm::vec3{0, 0, -5});
        scene_data.proj = glm::perspective(
            glm::radians(70.f),
            static_cast<float>(window_extent.width) / static_cast<float>(window_extent.height),
            10'000.f,
            0.1f
        );

        scene_data.proj[1][1] *= -1;
        scene_data.view_proj = scene_data.proj * scene_data.view;

        scene_data.ambient_color = glm::vec4{.1f};
        scene_data.sunlight_color = glm::vec4{1.f};
        scene_data.sunlight_direction = glm::vec4{0, 1, 0.5, 1.f};
    }

    void Engine::initializeWindow(
        const std::size_t width,
        const std::size_t height,
        const std::string_view title
    )
    {
        SDL_Init(SDL_INIT_VIDEO);
    
        SDL_WindowFlags window_flags {
            static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)
        };
    
        window = SDL_CreateWindow(
            title.data(),
            window_extent.width = width,
            window_extent.height = height,
            window_flags
        );
    }
    
    void Engine::initializeDescriptors()
    {
        std::vector<DescriptorAllocator::PoolSizeRatio> sizes {
            {
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                1
            }
        };
    
        global_descriptor_allocator.initialize(logical_device, 10, sizes);
    
        {
            DescriptorLayoutBuilder builder;
    
            builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    
            draw_image_descriptor_layout = builder.build(logical_device, VK_SHADER_STAGE_VERTEX_BIT);
        }
    
        {
            DescriptorLayoutBuilder builder;
    
            builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    
            scene_data_descriptor_layout = builder.build(
                logical_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            );
        }
    
        draw_image_descriptors = global_descriptor_allocator.allocate(logical_device, draw_image_descriptor_layout);
    
        {
            DescriptorWriter writer;
    
            writer.writeImage(
                0, draw_image.image_view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
            );
    
            writer.updateDescriptorSet(logical_device, draw_image_descriptors);
        }
    
        resource_cleaner.addCleaner(
            [&, this]
            {
                if(debug) std::println("Destroying desc allocator and desc layout");

                global_descriptor_allocator.destroyPools(logical_device);
    
                vkDestroyDescriptorSetLayout(logical_device, draw_image_descriptor_layout, nullptr);
                vkDestroyDescriptorSetLayout(logical_device, scene_data_descriptor_layout, nullptr);
            }
        );
    
        for(auto& frame : frames)
        {
            std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes {
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }
            };
    
            frame.frame_descriptors.initialize(logical_device, 1000, frame_sizes);
    
            resource_cleaner.addCleaner(
                [&, this]
                {
                    if(debug) std::println("Destroying pool");

                    frame.frame_descriptors.destroyPools(logical_device);
                }
            );
        }
    }
    
    
    
    void Engine::initializeInstance(const std::string_view app_name)
    {
        vkb::InstanceBuilder builder;
    
        const auto instance_ret {
            builder
            .set_app_name(app_name.data())
            .request_validation_layers(debug)
            .require_api_version(1, 4, 0)
            .build()
        };
    
        if(!instance_ret)
        {
            throw std::runtime_error{
                std::format("Failed to build Vulkan Instance ({})!", instance_ret.error().message())
            };
        }
    
        vkb_instance = instance_ret.value();
        
        instance = vkb_instance.instance;
    
        debug_messenger = vkb_instance.debug_messenger;    
    
        if(
            !SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)
        )
        {
            throw std::runtime_error{"Failed to create Vulkan surface!"};
        }
    }
    
    void Engine::initializePhysicalDevice()
    {
        vkb::PhysicalDeviceSelector physical_device_selector {vkb_instance};
    
        VkPhysicalDeviceVulkan13Features features_13 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
        };
    
        features_13.dynamicRendering = true;
        features_13.synchronization2 = true;
    
        VkPhysicalDeviceVulkan12Features features_12 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES 
        };
    
        features_12.bufferDeviceAddress = true;
        features_12.descriptorIndexing = true;
    
        const auto physical_device_ret {
            physical_device_selector
            .set_required_features_13(features_13)
            .set_required_features_12(features_12)
            .set_minimum_version(1, 4)
            .set_surface(surface)
            .select()
        };
    
        if(!physical_device_ret)
        {
            throw std::runtime_error{"Failed to find a suitable GPU!"};
        }
    
        vkb_physical_device = physical_device_ret.value();
    
        physical_device = vkb_physical_device.physical_device;
    }
    
    void Engine::initializeLogicalDevice()
    {
        vkb::DeviceBuilder device_builder {vkb_physical_device};
    
        const auto logical_device_ret {
            device_builder.build()
        };
    
        if(!logical_device_ret)
        {
            throw std::runtime_error{"Failed to create Vulkan logical device!"};
        }
    
        vkb_device = logical_device_ret.value();
        
        logical_device = vkb_device.device;
    }
    
    void Engine::initializeQueues()
    {
        graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    
        graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
    }
    
    void Engine::initializeAllocator()
    {
        VmaAllocatorCreateInfo allocator_info {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physical_device,
            .device = logical_device,
            .instance = instance
        };
    
        if(vmaCreateAllocator(&allocator_info, &allocator) != VK_SUCCESS)
        {
            throw std::runtime_error{"Failed to create VMA allocator!"};
        }
    
        resource_cleaner.addCleaner(
            [this]
            {
                if(debug) std::println("Destroying vma");

                vmaDestroyAllocator(allocator);
            }
        );
    }
    
    Engine::~Engine()
    {
        cleanup();
    }
    
    void Engine::createSwapchain(const std::size_t width, const std::size_t height)
    {
        vkb::SwapchainBuilder swapchain_builder {
            physical_device,
            logical_device,
            surface
        };
    
        swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
    
        VkSurfaceFormatKHR surface_format;
    
        surface_format.format = swapchain_image_format;
        surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    
        const auto vkb_swapchain_ret {
            swapchain_builder
            .set_desired_format(surface_format)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
        };
    
        if(!vkb_swapchain_ret)
        {
            throw std::runtime_error{"Failed to create swapchain!"};
        }
    
        auto vkb_swapchain {vkb_swapchain_ret.value()};
    
        swapchain_extent = vkb_swapchain.extent;
        swapchain = vkb_swapchain.swapchain;
        swapchain_images = vkb_swapchain.get_images().value();
        swapchain_image_views = vkb_swapchain.get_image_views().value();
    }
    
    
    
    void Engine::initializeSwapchain()
    {
        createSwapchain(window_extent.width, window_extent.height);
    
        VkExtent3D draw_image_extent {
            window_extent.width,
            window_extent.height,
            1
        };
    
        draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        draw_image.image_extent = draw_image_extent;
    
        VkImageUsageFlags draw_image_usages {};
    
        draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
        draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
        VkImageCreateInfo image_info
        {
            generateImageCreateInfo(
                draw_image.image_format,
                draw_image_usages,
                draw_image_extent
            )
        };
    
        VmaAllocationCreateInfo image_allocate_info {};
    
        image_allocate_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        image_allocate_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
        check(
            vmaCreateImage(
                allocator,
                &image_info,
                &image_allocate_info,
                &draw_image.image,
                &draw_image.allocation,
                nullptr
            )
        );
    
        VkImageViewCreateInfo image_view_info {
            generateImageViewCreateInfo(
                draw_image.image_format,
                draw_image.image,
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        };
    
        check(vkCreateImageView(logical_device, &image_view_info, nullptr, &draw_image.image_view));
    
        depth_image.image_format = VK_FORMAT_D32_SFLOAT;
        depth_image.image_extent = draw_image_extent;
    
        VkImageUsageFlags depth_image_usages {};
    
        depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
        VkImageCreateInfo depth_image_info {
            generateImageCreateInfo(
                depth_image.image_format,
                depth_image_usages,
                draw_image_extent
            )
        };
    
        vmaCreateImage(
            allocator,
            &depth_image_info,
            &image_allocate_info,
            &depth_image.image,
            &depth_image.allocation,
            nullptr
        );
    
        VkImageViewCreateInfo depth_image_view_info {
            generateImageViewCreateInfo(
                depth_image.image_format,
                depth_image.image,
                VK_IMAGE_ASPECT_DEPTH_BIT
            )
        };
    
        check(
            vkCreateImageView(
                logical_device, &depth_image_view_info, nullptr, &depth_image.image_view
            )
        );
    
        resource_cleaner.addCleaner(
            [=, this]
            {
                if(debug) std::println("Destroying images");

                destroyImage(depth_image);
                destroyImage(draw_image);
            }
        );
    }
    
    void Engine::cleanup()
    {
        vkDeviceWaitIdle(logical_device);
    
        for(auto& frame : frames)
        {
            vkDestroyCommandPool(logical_device, frame.command_pool, nullptr);
    
            vkDestroyFence(logical_device, frame.render_fence, nullptr);
            vkDestroySemaphore(logical_device, frame.render_semaphore, nullptr);
            vkDestroySemaphore(logical_device, frame.swapchain_semaphore, nullptr);
        
            frame.resource_cleaner.flush();
        }
    
        for(auto& mesh : test_meshes)
        {
            destroyBuffer(mesh->mesh_buffers.index_buffer);
            destroyBuffer(mesh->mesh_buffers.vertex_buffer);
        }

        metal_rough_material.clearResources(logical_device);

        resource_cleaner.flush();
    
        destroySwapchain();
        
        vkDestroySurfaceKHR(instance, surface, nullptr);
    
        vkDestroyDevice(logical_device, nullptr);
    
        vkb::destroy_debug_utils_messenger(instance, debug_messenger);
        
        vkDestroyInstance(instance, nullptr);
    
        SDL_DestroyWindow(window);
    }
    
    FrameData& Engine::getCurrentFrame()
    {
        return frames[frame_number % frame_overlap];
    }
    
    void Engine::initializeCommands()
    {
        VkCommandPoolCreateInfo command_pool_info {
            generateCommandPoolCreateInfo(
                graphics_queue_family,
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            )
        };
    
        for(auto& frame : frames)
        {
            check(
                vkCreateCommandPool(
                    logical_device, 
                    &command_pool_info,
                    nullptr,
                    &frame.command_pool
                )
            );
    
            VkCommandBufferAllocateInfo command_buffer_allocate_info {
                generateCommandBufferAllocateInfo(
                    frame.command_pool, 1
                )
            };
    
            check(
                vkAllocateCommandBuffers(
                    logical_device,
                    &command_buffer_allocate_info,
                    &frame.main_command_buffer
                )
            );
        }
    
        check(
            vkCreateCommandPool(
                logical_device,
                &command_pool_info,
                nullptr,
                &immediate_command_pool
            )
        );
    
        VkCommandBufferAllocateInfo command_buffer_allocate_info {
            generateCommandBufferAllocateInfo(
                immediate_command_pool, 1
            )
        };
    
        check(
            vkAllocateCommandBuffers(
                logical_device,
                &command_buffer_allocate_info,
                &immediate_command_buffer
            )
        );
    
        resource_cleaner.addCleaner(
            [=, this]
            {
                if(debug) std::println("Destroying command pool");

                vkDestroyCommandPool(logical_device, immediate_command_pool, nullptr);
            }
        );
    }
    
    void Engine::initializeSyncStructures()
    {
        VkFenceCreateInfo fence_create_info {
            generateFenceCreateinfo(VK_FENCE_CREATE_SIGNALED_BIT)
        };
    
        VkSemaphoreCreateInfo semaphore_create_info {
            generateSemaphoreCreateInfo()
        };
    
        for(auto& frame : frames)
        {
            check(
                vkCreateFence(
                    logical_device, &fence_create_info, nullptr, &frame.render_fence
                )
            );
    
            check(
                vkCreateSemaphore(
                    logical_device, &semaphore_create_info, nullptr, &frame.swapchain_semaphore
                )
            );
    
            check(
                vkCreateSemaphore(
                    logical_device, &semaphore_create_info, nullptr, &frame.render_semaphore
                )
            );
        }
    
        check(
            vkCreateFence(
                logical_device,
                &fence_create_info,
                nullptr,
                &immediate_fence
            )
        );
    
        resource_cleaner.addCleaner(
            [this]
            {
                if(debug) std::println("Destroying fence");

                vkDestroyFence(logical_device, immediate_fence, nullptr);
            }
        );
    }
    
    void Engine::draw()
    {
        updateScene();

        check(
            vkWaitForFences(
                logical_device, 1, &getCurrentFrame().render_fence, true, 1'000'000'000
            )
        );
    
        getCurrentFrame().resource_cleaner.flush();
        getCurrentFrame().frame_descriptors.clearPools(logical_device);
    
        std::uint32_t swapchain_image_index;
    
        if(
            vkAcquireNextImageKHR(
                logical_device,
                swapchain,
                1'000'000'000,
                getCurrentFrame().swapchain_semaphore,
                nullptr,
                &swapchain_image_index
            )
            ==
            VK_ERROR_OUT_OF_DATE_KHR
        )
        {
            resize_requested = true;
    
            return;
        }

        draw_extent.width = std::min(swapchain_extent.width, draw_image.image_extent.width) * render_scale;
        draw_extent.height = std::min(swapchain_extent.width, draw_image.image_extent.height) * render_scale;    
        
    
        check(
            vkResetFences(
                logical_device, 1, &getCurrentFrame().render_fence
            )
        );
    
        VkCommandBuffer command_buffer {getCurrentFrame().main_command_buffer};
    
        check(vkResetCommandBuffer(command_buffer, 0));
    
        VkCommandBufferBeginInfo command_buffer_begin_info {
            generateCommandBufferBeginInfo(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            )
        };
    
        check(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
    
        changeImageLayout(
            command_buffer,
            draw_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL
        );
    
        //drawBackground(command_buffer);
    
        changeImageLayout(
            command_buffer,
            draw_image.image,
            VK_IMAGE_LAYOUT_GENERAL, 
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
    
        changeImageLayout(
            command_buffer, 
            depth_image.image, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        );
    
        drawGeometry(command_buffer);
    
        changeImageLayout(
            command_buffer,
            draw_image.image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
    
        changeImageLayout(
            command_buffer,
            swapchain_images[swapchain_image_index],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
    
        copyImage(
            command_buffer,
            draw_image.image,
            swapchain_images[swapchain_image_index],
            draw_extent,
            swapchain_extent
        );
    
        changeImageLayout(
            command_buffer,
            swapchain_images[swapchain_image_index],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );    
    
        check(
            vkEndCommandBuffer(command_buffer)
        );
    
        VkCommandBufferSubmitInfo command_buffer_info {
            generateCommandBufferSubmitInfo(command_buffer)
        };
    
        VkSemaphoreSubmitInfo wait_info {
            generateSemaphoreSubmitInfo(
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                getCurrentFrame().swapchain_semaphore
            )
        };
    
        VkSemaphoreSubmitInfo signal_info {
            generateSemaphoreSubmitInfo(
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                getCurrentFrame().render_semaphore
            )
        };
    
        VkSubmitInfo2 submit_info {
            generateSubmitInfo(
                &command_buffer_info,
                &signal_info,
                &wait_info
            )
        };
    
        check(
            vkQueueSubmit2(
                graphics_queue,
                1,
                &submit_info,
                getCurrentFrame().render_fence
            )
        );
    
        VkPresentInfoKHR present_info {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr
        };
    
        present_info.pSwapchains = &swapchain;
        present_info.swapchainCount = 1;
    
        present_info.pWaitSemaphores = &getCurrentFrame().render_semaphore;
        present_info.waitSemaphoreCount = 1;
    
        present_info.pImageIndices = &swapchain_image_index;
    
        if(
            vkQueuePresentKHR(graphics_queue, &present_info)
            ==
            VK_ERROR_OUT_OF_DATE_KHR
        )
        {
            resize_requested = true;
        }
    
        ++frame_number;
    }
    
    bool Engine::resizeRequested()
    {
        return resize_requested;
    }
    
    void Engine::destroySwapchain()
    {
        for(auto& image_view : swapchain_image_views)
        {
            vkDestroyImageView(
                logical_device,
                image_view,
                nullptr
            );
        }
    
        vkDestroySwapchainKHR(
            logical_device, swapchain, nullptr
        );    
    }
    
    void Engine::resizeSwapchain()
    {
        vkDeviceWaitIdle(logical_device);
    
        destroySwapchain();
    
        int width;
        int height;
    
        SDL_GetWindowSize(window, &width, &height);
    
        window_extent.width = width;
        window_extent.height = height;
    
        createSwapchain(window_extent.width, window_extent.height);
    
        resize_requested = false;
    }
    
    void Engine::drawBackground(const VkCommandBuffer command_buffer)
    {
        VkClearColorValue clear_value;
    
        clear_value = {
            {0.0f, 0.0f, 0.0f, 1.0f}
        };
    
        VkImageSubresourceRange clear_range {
            generateImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
        };
    
        vkCmdClearColorImage(
            command_buffer,
            draw_image.image,
            VK_IMAGE_LAYOUT_GENERAL,
            &clear_value,
            1,
            &clear_range
        );
    }
    
    void PipelineBuilder::enableDepthTest(
        const bool depth_write_enable, const VkCompareOp op
    )
    {
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = depth_write_enable;
        depth_stencil.depthCompareOp = op;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;
        depth_stencil.front = {};
        depth_stencil.back = {};
        depth_stencil.minDepthBounds = 0.f;
        depth_stencil.maxDepthBounds = 1.f;
    }
    
    AllocatedImage Engine::createImage(
        const VkExtent3D size,
        const VkFormat format,
        const VkImageUsageFlags usage,
        bool mipmapped 
    )
    {
        AllocatedImage new_image;
    
        new_image.image_format = format;
        new_image.image_extent = size;
    
        VkImageCreateInfo image_info {
            generateImageCreateInfo(format, usage, size)
        };
    
        if(mipmapped)
        {
            image_info.mipLevels = static_cast<std::uint32_t>(
                std::floor(std::log2(std::max(size.width, size.height))) + 1
            );
        }
    
        VmaAllocationCreateInfo allocate_info {};
    
        allocate_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocate_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
        check(
            vmaCreateImage(
                allocator, &image_info, &allocate_info, &new_image.image, &new_image.allocation, nullptr
            )
        );
    
        VkImageAspectFlags aspect_flag {VK_IMAGE_ASPECT_COLOR_BIT};
    
        if(format == VK_FORMAT_D32_SFLOAT)
        {
            aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
    
        VkImageViewCreateInfo view_info {
            generateImageViewCreateInfo(
                format, new_image.image, aspect_flag
            )
        };
    
        view_info.subresourceRange.levelCount = image_info.mipLevels;
    
        check(
            vkCreateImageView(
                logical_device, &view_info, nullptr, &new_image.image_view
            )
        );
    
        return new_image;
    }
    
    AllocatedImage Engine::createImage(
        const void* const data,
        const VkExtent3D size,
        const VkFormat format,
        const VkImageUsageFlags usage,
        bool mipmapped            
    )
    {
        std::size_t data_size {
            size.depth * size.width * size.height * 4
        };
    
        AllocatedBuffer upload_buffer {
            createBuffer(
                data_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU
            )
        };
    
        std::memcpy(upload_buffer.allocation_info.pMappedData, data, data_size);
    
        AllocatedImage new_image {
            createImage(
                size, 
                format,
                usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                mipmapped
            )
        };
    
        immediateSubmit(
            [&, this](const VkCommandBuffer command_buffer)
            {
                changeImageLayout(
                    command_buffer, 
                    new_image.image, 
                    VK_IMAGE_LAYOUT_UNDEFINED, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                );
    
                VkBufferImageCopy copy_region {};
    
                copy_region.bufferOffset = 0;
                copy_region.bufferRowLength = 0;
                copy_region.bufferImageHeight = 0;
    
                copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy_region.imageSubresource.mipLevel = 0;
                copy_region.imageSubresource.baseArrayLayer = 0;
                copy_region.imageSubresource.layerCount = 1;
                copy_region.imageExtent = size;
    
                vkCmdCopyBufferToImage(
                    command_buffer, 
                    upload_buffer.buffer, 
                    new_image.image, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                    1,
                    &copy_region 
                );
    
                changeImageLayout(
                    command_buffer, 
                    new_image.image, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            }
        );
    
        destroyBuffer(upload_buffer);
    
        return new_image;
    }
    
    void Engine::destroyImage(const AllocatedImage& image)
    {
        vkDestroyImageView(logical_device, image.image_view, nullptr);
        vmaDestroyImage(allocator, image.image, image.allocation);
    }
    
    void Engine::drawGeometry(const VkCommandBuffer command_buffer)
    {
        VkRenderingAttachmentInfo color_attachment {
            generateAttachmentInfo(
                draw_image.image_view,
                nullptr,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            )
        };
    
        VkRenderingAttachmentInfo depth_attachment {
            generateDepthAttachmentInfo(
                depth_image.image_view,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            )
        };
    
        VkRenderingInfo render_info {
            generateRenderingInfo(
                draw_extent, &color_attachment, &depth_attachment
            )
        };
    
        vkCmdBeginRendering(command_buffer, &render_info);
    
        VkViewport viewport {};
    
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = draw_extent.width;
        viewport.height = draw_extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
    
        vkCmdSetViewport(
            command_buffer,
            0,
            1,
            &viewport
        );
    
        VkRect2D scissor {};
    
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = draw_extent.width;
        scissor.extent.height = draw_extent.height;
    
        vkCmdSetScissor(
            command_buffer, 0, 1, &scissor
        );
        
        AllocatedBuffer scene_data_buffer {
            createBuffer(
                sizeof(SceneData), 
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VMA_MEMORY_USAGE_CPU_TO_GPU
            )
        };

        getCurrentFrame().resource_cleaner.addCleaner(
            [=, this]
            {
                if(debug) std::println("Destroying buffer");

                destroyBuffer(scene_data_buffer);
            }
        );

        SceneData* scene_uniform_data {
            reinterpret_cast<SceneData*>(scene_data_buffer.allocation->GetMappedData())
        };

        *scene_uniform_data = scene_data;
    
        VkDescriptorSet global_descriptor {
            getCurrentFrame().frame_descriptors.allocate(logical_device, scene_data_descriptor_layout)
        };

        DescriptorWriter writer;

        writer.writeBuffer(
            0, 
            scene_data_buffer.buffer, 
            sizeof(SceneData), 
            0, 
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        );

        writer.updateDescriptorSet(logical_device, global_descriptor);

        for(const auto& object : main_draw_context.opaque_surfaces)
        {
            vkCmdBindPipeline(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                object.material->pipeline->pipeline
            );

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                object.material->pipeline->layout,
                0,
                1,
                &global_descriptor,
                0,
                nullptr
            );

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                object.material->pipeline->layout,
                1,
                1,
                &object.material->descriptor_set,
                0,
                nullptr
            );
    
            vkCmdBindIndexBuffer(command_buffer, object.index_buffer, 0, VK_INDEX_TYPE_UINT32);
    
            DrawPushCostants pushConstants;

            pushConstants.vertex_buffer = object.vertex_buffer_address;
            pushConstants.world_matrix = object.transform;

            vkCmdPushConstants(
                command_buffer,
                object.material->pipeline->layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(DrawPushCostants),
                &pushConstants
            );
    
            vkCmdDrawIndexed(command_buffer, object.index_count, 1, object.first_index, 0, 0);
        }

        vkCmdEndRendering(command_buffer);
    }   
    
    AllocatedBuffer Engine::createBuffer(const std::size_t allocate_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
    {
        VkBufferCreateInfo buffer_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr
        };
    
        buffer_info.size = allocate_size;
        buffer_info.usage = usage;
    
        VmaAllocationCreateInfo vma_allocation_info {};
    
        vma_allocation_info.usage = memory_usage;
        vma_allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
        AllocatedBuffer new_buffer;
    
        check(
            vmaCreateBuffer(
                allocator,
                &buffer_info,
                &vma_allocation_info,
                &new_buffer.buffer,
                &new_buffer.allocation,
                &new_buffer.allocation_info
            )
        );
    
        return new_buffer;
    }
    
    void Engine::destroyBuffer(const AllocatedBuffer &buffer)
    {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    }
    
    void Engine::immediateSubmit(const std::function<void(const VkCommandBuffer command_buffer)> &&function)
    {
        check(
            vkResetFences(logical_device, 1, &immediate_fence)
        );
    
        check(
            vkResetCommandBuffer(immediate_command_buffer, 0)
        );
    
        VkCommandBufferBeginInfo command_buffer_begin_info {
            generateCommandBufferBeginInfo(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            )
        };
    
        check(
            vkBeginCommandBuffer(immediate_command_buffer, &command_buffer_begin_info)
        );
    
        function(immediate_command_buffer);
    
        check(
            vkEndCommandBuffer(immediate_command_buffer)
        );
    
        VkCommandBufferSubmitInfo command_buffer_submit_info {
            generateCommandBufferSubmitInfo(immediate_command_buffer)
        };
    
        VkSubmitInfo2 submit_info {
            generateSubmitInfo(&command_buffer_submit_info, nullptr, nullptr)
        };
    
        check(
            vkQueueSubmit2(
                graphics_queue, 1, &submit_info, immediate_fence
            )
        );
    
        check(
            vkWaitForFences(
                logical_device, 1, &immediate_fence, true, 9'999'999'999
            )
        );
    }
    
    MeshBuffers Engine::uploadMesh(const std::span<std::uint32_t> indices, const std::span<Vertex> vertices)
    {
        const std::size_t vertex_buffer_size {
            vertices.size() * sizeof(Vertex)
        };
    
        const std::size_t index_buffer_size {
            indices.size() * sizeof(std::uint32_t)
        };
    
        MeshBuffers new_surface;
    
        new_surface.vertex_buffer = createBuffer(
            vertex_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
    
        VkBufferDeviceAddressInfo device_address_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr
        };
    
        device_address_info.buffer = new_surface.vertex_buffer.buffer;
    
        new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(
            logical_device, &device_address_info
        );
    
        new_surface.index_buffer = createBuffer(
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
    
        AllocatedBuffer staging {
            createBuffer(
                vertex_buffer_size + index_buffer_size, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY
            )
        };
    
        void* data {staging.allocation->GetMappedData()};
    
        std::memcpy(data, vertices.data(), vertex_buffer_size);
        std::memcpy(
            static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size
        );
    
        immediateSubmit(
            [&](const VkCommandBuffer command_buffer)
            {
                VkBufferCopy vertex_copy {};
    
                vertex_copy.dstOffset = 0;
                vertex_copy.srcOffset = 0;
                vertex_copy.size = vertex_buffer_size;
    
                vkCmdCopyBuffer(
                    command_buffer,
                    staging.buffer,
                    new_surface.vertex_buffer.buffer,
                    1,
                    &vertex_copy
                );
    
                VkBufferCopy index_copy {};
    
                index_copy.dstOffset = 0;
                index_copy.srcOffset = vertex_buffer_size;
                index_copy.size = index_buffer_size;
    
                vkCmdCopyBuffer(
                    command_buffer,
                    staging.buffer,
                    new_surface.index_buffer.buffer,
                    1,
                    &index_copy
                );
            }
        );
    
        destroyBuffer(staging);
    
        return new_surface;
    }
    
}