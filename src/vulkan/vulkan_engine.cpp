#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#define VMA_IMPLEMENTATION

#include "vk_mem_alloc.h"

#include "vulkan_engine.hpp"
#include "descriptor_layout_builder.hpp"
#include "descriptor_writer.hpp"
#include "pipeline_builder.hpp"
#include <cstddef>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>

#include <vulkan/vk_enum_string_helper.h>

#include <print>
#include <format>
#include <cmath>

VulkanEngine::VulkanEngine(
    const std::string_view app_name,
    const std::size_t window_width,
    const std::size_t window_height,
    const std::string_view window_title
)
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
    initializeTrianglePipeline();
    initializeMeshPipeline();
    initializeDefaultData();
}

void VulkanEngine::initializeWindow(
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

void VulkanEngine::initializeDescriptors()
{
    std::vector<GrowableDescriptorAllocator::PoolSizeRatio> sizes {
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

        writer.updateSet(logical_device, draw_image_descriptors);
    }

    deletors.addDeletor(
        [&, this]
        {
            global_descriptor_allocator.destroyPools(logical_device);

            vkDestroyDescriptorSetLayout(logical_device, draw_image_descriptor_layout, nullptr);
            vkDestroyDescriptorSetLayout(logical_device, scene_data_descriptor_layout, nullptr);
        }
    );

    for(auto& frame : frames)
    {
        std::vector<GrowableDescriptorAllocator::PoolSizeRatio> frame_sizes {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }
        };

        frame.frame_descriptors.initialize(logical_device, 1000, frame_sizes);

        deletors.addDeletor(
            [&, this]
            {
                frame.frame_descriptors.destroyPools(logical_device);
            }
        );
    }
}



void VulkanEngine::initializeInstance(const std::string_view app_name)
{
    vkb::InstanceBuilder builder;

    const auto instance_ret {
        builder
        .set_app_name(app_name.data())
        .request_validation_layers(use_validation_layers)
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

void VulkanEngine::initializePhysicalDevice()
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

void VulkanEngine::initializeLogicalDevice()
{
    vkb::DeviceBuilder device_builder {vkb_physical_device};

    const auto logical_device_ret {
        device_builder.build()
    };

    if(!logical_device_ret)
    {
        throw std::runtime_error{"Failed to create Vulkan logical device!"};
    }

    vkb_logical_device = logical_device_ret.value();
    
    logical_device = vkb_logical_device.device;
}

void VulkanEngine::initializeQueues()
{
    graphics_queue = vkb_logical_device.get_queue(vkb::QueueType::graphics).value();

    graphics_queue_family = vkb_logical_device.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initializeAllocator()
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

    deletors.addDeletor(
        [this]
        {
            vmaDestroyAllocator(allocator);
        }
    );
}

VulkanEngine::~VulkanEngine()
{
    cleanup();
}

void VulkanEngine::createSwapchain(const std::size_t width, const std::size_t height)
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



void VulkanEngine::initializeSwapchain()
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

    deletors.addDeletor(
        [=, this]
        {
            destroyImage(depth_image);
            destroyImage(draw_image);
        }
    );
}

void VulkanEngine::cleanup()
{
    vkDeviceWaitIdle(logical_device);

    for(auto& frame : frames)
    {
        vkDestroyCommandPool(logical_device, frame.command_pool, nullptr);

        vkDestroyFence(logical_device, frame.render_fence, nullptr);
        vkDestroySemaphore(logical_device, frame.render_semaphore, nullptr);
        vkDestroySemaphore(logical_device, frame.swapchain_semaphore, nullptr);
    
        frame.deletors.flush();
    }

    destroyBuffer(rectangle.index_buffer);
    destroyBuffer(rectangle.vertex_buffer);

    deletors.flush();

    destroySwapchain();
    
    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyDevice(logical_device, nullptr);

    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
    
    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
}

FrameData& VulkanEngine::getCurrentFrame()
{
    return frames[frame_number % frame_overlap];
}

void VulkanEngine::initializeCommands()
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

    deletors.addDeletor(
        [=, this]
        {
            vkDestroyCommandPool(logical_device, immediate_command_pool, nullptr);
        }
    );
}

void VulkanEngine::initializeSyncStructures()
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

    deletors.addDeletor(
        [this]
        {
            vkDestroyFence(logical_device, immediate_fence, nullptr);
        }
    );
}

void VulkanEngine::draw()
{
    check(
        vkWaitForFences(
            logical_device, 1, &getCurrentFrame().render_fence, true, 1'000'000'000
        )
    );

    getCurrentFrame().deletors.flush();
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

    draw_extent.width = std::min(swapchain_extent.width, draw_image.image_extent.width) * render_scale;
    draw_extent.height = std::min(swapchain_extent.width, draw_image.image_extent.height) * render_scale;    

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

bool VulkanEngine::resizeRequested()
{
    return resize_requested;
}

void VulkanEngine::initializeTrianglePipeline()
{
    VkShaderModule triangle_fragment_shader;

    if(
        !loadShaderModule(
            "../shaders/colored_triangle.frag.spv",
            logical_device,
            &triangle_fragment_shader
        )
    )
    {
        throw std::runtime_error{"Failed to load main fragment shader!"};
    }

    VkShaderModule triangle_vertex_shader;

    if(
        !loadShaderModule(
            "../shaders/colored_triangle.vert.spv",
            logical_device,
            &triangle_vertex_shader
        )
    )
    {
        throw std::runtime_error{"Failed to load main vertex shader!"};
    }    

    VkPipelineLayoutCreateInfo pipeline_layout_info {
        generatePipelineLayoutCreateInfo()
    };

    check(
        vkCreatePipelineLayout(
            logical_device,
            &pipeline_layout_info,
            nullptr,
            &triangle_pipeline_layout
        )
    );

    PipelineBuilder pipeline_builder;

    pipeline_builder.pipeline_layout = triangle_pipeline_layout;
    
    pipeline_builder.setShaders(triangle_vertex_shader, triangle_fragment_shader);

    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

    pipeline_builder.setMultisamplingToNone();
    pipeline_builder.disableBlending();
    pipeline_builder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder.setColorAttachmentFormat(draw_image.image_format);
    pipeline_builder.setDepthFormat(depth_image.image_format);

    triangle_pipeline = pipeline_builder.build(logical_device);

    vkDestroyShaderModule(logical_device, triangle_fragment_shader, nullptr);
    vkDestroyShaderModule(logical_device, triangle_vertex_shader, nullptr);

    deletors.addDeletor(
        [&, this]
        {
            vkDestroyPipelineLayout(logical_device, triangle_pipeline_layout, nullptr);
            vkDestroyPipeline(logical_device, triangle_pipeline, nullptr);
        }
    );
}

void VulkanEngine::initializeMeshPipeline()
{
    VkShaderModule triangle_fragment_shader;

    if(
        !loadShaderModule(
            "../shaders/colored_triangle.frag.spv",
            logical_device,
            &triangle_fragment_shader
        )
    )
    {
        throw std::runtime_error{"Failed to load triangle fragment shader!"};
    }

    VkShaderModule triangle_mesh_vertex_shader;

    if(
        !loadShaderModule(
            "../shaders/mesh.vert.spv",
            logical_device,
            &triangle_mesh_vertex_shader
        )
    )
    {
        throw std::runtime_error{"Failed to load mesh vertex shader!"};
    }    

    VkPushConstantRange buffer_range {};

    buffer_range.offset = 0;
    buffer_range.size = sizeof(GPUDrawPushCostants);
    buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info {
        generatePipelineLayoutCreateInfo()
    };

    pipeline_layout_info.pPushConstantRanges = &buffer_range;
    pipeline_layout_info.pushConstantRangeCount = 1;

    check(
        vkCreatePipelineLayout(
            logical_device,
            &pipeline_layout_info,
            nullptr,
            &mesh_pipeline_layout
        )
    );

    PipelineBuilder pipeline_builder;

    pipeline_builder.pipeline_layout = mesh_pipeline_layout;
    
    pipeline_builder.setShaders(triangle_mesh_vertex_shader, triangle_fragment_shader);

    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

    pipeline_builder.setMultisamplingToNone();
    pipeline_builder.disableBlending();
    pipeline_builder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder.setColorAttachmentFormat(draw_image.image_format);
    pipeline_builder.setDepthFormat(depth_image.image_format);

    mesh_pipeline = pipeline_builder.build(logical_device);

    vkDestroyShaderModule(logical_device, triangle_fragment_shader, nullptr);
    vkDestroyShaderModule(logical_device, triangle_mesh_vertex_shader, nullptr);

    deletors.addDeletor(
        [&, this]
        {
            vkDestroyPipelineLayout(logical_device, mesh_pipeline_layout, nullptr);
            vkDestroyPipeline(logical_device, mesh_pipeline, nullptr);
        }
    );
}

void VulkanEngine::initializeDefaultData()
{
    std::array<Vertex, 4> rectangle_vertices;

	rectangle_vertices[0].position = {0.5,-0.5, 0};
	rectangle_vertices[1].position = {0.5,0.5, 0};
	rectangle_vertices[2].position = {-0.5,-0.5, 0};
	rectangle_vertices[3].position = {-0.5,0.5, 0};

	rectangle_vertices[0].color = {0,0, 0,1};
	rectangle_vertices[1].color = { 0.5,0.5,0.5 ,1};
	rectangle_vertices[2].color = { 1,0, 0,1 };
	rectangle_vertices[3].color = { 0,1, 0,1 };

	std::array<std::uint32_t, 6> rectangle_indices;

	rectangle_indices[0] = 0;
	rectangle_indices[1] = 1;
	rectangle_indices[2] = 2;

	rectangle_indices[3] = 2;
	rectangle_indices[4] = 1;
	rectangle_indices[5] = 3;

    rectangle = uploadMesh(rectangle_indices, rectangle_vertices);
}

void VulkanEngine::destroySwapchain()
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

void VulkanEngine::resizeSwapchain()
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

void VulkanEngine::drawBackground(const VkCommandBuffer command_buffer)
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

AllocatedImage VulkanEngine::createImage(
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

AllocatedImage VulkanEngine::createImage(
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

void VulkanEngine::destroyImage(const AllocatedImage& image)
{
    vkDestroyImageView(logical_device, image.image_view, nullptr);
    vmaDestroyImage(allocator, image.image, image.allocation);
}

void VulkanEngine::drawGeometry(const VkCommandBuffer command_buffer)
{
    AllocatedBuffer scene_data_buffer {
        createBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)
    };

    getCurrentFrame().deletors.addDeletor(
        [=, this]
        {   
            destroyBuffer(scene_data_buffer);
        }
    );

    SceneData* scene_uniform_data {
        reinterpret_cast<SceneData*>(scene_data_buffer.allocation->GetMappedData())
    };

    *scene_uniform_data = scene_data;

    VkDescriptorSet global_descriptor {
        getCurrentFrame().frame_descriptors.allocate(
            logical_device, scene_data_descriptor_layout)
    };

    DescriptorWriter writer;

    writer.writeBuffer(
        0, scene_data_buffer.buffer, sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    );

    writer.updateSet(logical_device, global_descriptor);

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

    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        triangle_pipeline
    );

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

    vkCmdDraw(
        command_buffer, 3, 1, 0, 0
    );

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

    GPUDrawPushCostants push_constants;

    push_constants.world_matrix = glm::mat4{1.f};
    push_constants.vertex_buffer = rectangle.vertex_buffer_address;

    vkCmdPushConstants(
        command_buffer,
        mesh_pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(GPUDrawPushCostants),
        &push_constants
    );

    vkCmdBindIndexBuffer(
        command_buffer,
        rectangle.index_buffer.buffer,
        0,
        VK_INDEX_TYPE_UINT32
    );

    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

    vkCmdEndRendering(command_buffer);
}   

AllocatedBuffer VulkanEngine::createBuffer(const std::size_t allocate_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
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

void VulkanEngine::destroyBuffer(const AllocatedBuffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::immediateSubmit(const std::function<void(const VkCommandBuffer command_buffer)> &&function)
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

MeshBuffers VulkanEngine::uploadMesh(const std::span<std::uint32_t> indices, const std::span<Vertex> vertices)
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
