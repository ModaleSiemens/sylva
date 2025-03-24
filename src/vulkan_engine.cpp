#include "vulkan_engine.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vulkan/vk_enum_string_helper.h>

#include <iostream>
#include <print>
#include <format>
#include <cmath>
#include <fstream>
#include <filesystem>

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
        static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN)
    };

    window = SDL_CreateWindow(
        title.data(),
        window_extent.width = width,
        window_extent.height = height,
        window_flags
    );

    deletors.addDeletor(
        [this]
        {
            SDL_DestroyWindow(window);
        }
    );
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

    deletors.addDeletor(
        [this]
        {
            vkb::destroy_debug_utils_messenger(instance, debug_messenger);
            vkDestroyInstance(instance, nullptr);
        }
    );
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

    deletors.addDeletor(
        [this]
        {
            vkDestroyDevice(logical_device, nullptr);
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
    );
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

    deletors.addDeletor(
        [this]
        {
            vkDestroySwapchainKHR(logical_device, swapchain, nullptr);

            for(const auto& image_view : swapchain_image_views)
            {
                vkDestroyImageView(logical_device, image_view, nullptr);
            }
        }
    );
}

VkImageCreateInfo vulkan_utils::generateImageCreateInfo(
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

VkImageViewCreateInfo vulkan_utils::generateImageViewCreateInfo(
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
        vulkan_utils::generateImageCreateInfo(
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
        vulkan_utils::generateImageViewCreateInfo(
            draw_image.image_format,
            draw_image.image,
            VK_IMAGE_ASPECT_COLOR_BIT
        )
    };

    check(vkCreateImageView(logical_device, &image_view_info, nullptr, &draw_image.image_view));

    deletors.addDeletor(
        [this]
        {
            vkDestroyImageView(logical_device, draw_image.image_view, nullptr);
            vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);
        }
    );
}

void VulkanEngine::cleanup()
{
    vkDeviceWaitIdle(logical_device);

    deletors.flush();
}

void VulkanEngine::check(const VkResult result)
{
    if(result)
    {
        std::println("Detected Vulkan error: {}.", string_VkResult(result));
        std::abort();
    }
}

vulkan_utils::FrameData& VulkanEngine::getCurrentFrame()
{
    return frames[frame_number % frame_overlap];
}

VkCommandPoolCreateInfo vulkan_utils::generateCommandPoolCreateInfo(const std::uint32_t queue_family_index, const VkCommandPoolCreateFlags flags)
{
    VkCommandPoolCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr
    };

    info.queueFamilyIndex = queue_family_index;
    info.flags = flags;

    return info;
}

VkCommandBufferAllocateInfo vulkan_utils::generateCommandBufferAllocateInfo(const VkCommandPool pool, const std::uint32_t count)
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

VkFenceCreateInfo vulkan_utils::generateFenceCreateinfo(const VkFenceCreateFlags flags)
{
    VkFenceCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr
    };

    info.flags = flags;

    return info;
}

VkSemaphoreCreateInfo vulkan_utils::generateSemaphoreCreateInfo(const VkSemaphoreCreateFlags flags)
{
    VkSemaphoreCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr
    };

    info.flags = flags;

    return info;
}

VkCommandBufferBeginInfo vulkan_utils::generateCommandBufferBeginInfo(const VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr
    };

    info.pInheritanceInfo = nullptr;
    
    info.flags = flags;

    return info;
}

VkImageSubresourceRange vulkan_utils::generateImageSubresourceRange(const VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange sub_image {};

    sub_image.aspectMask = aspect_mask;
    sub_image.baseMipLevel = 0;
    sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
    sub_image.baseArrayLayer = 0;
    sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return sub_image;
}

VkSemaphoreSubmitInfo vulkan_utils::generateSemaphoreSubmitInfo(const VkPipelineStageFlags2 stage_mask, const VkSemaphore semaphore)
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

VkCommandBufferSubmitInfo vulkan_utils::generateCommandBufferSubmitInfo(const VkCommandBuffer command_buffer)
{
    VkCommandBufferSubmitInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr
    };

    info.commandBuffer = command_buffer;
    info.deviceMask = 0;

    return info;
}

VkSubmitInfo2 vulkan_utils::generateSubmitInfo(const VkCommandBufferSubmitInfo *command_buffer, const VkSemaphoreSubmitInfo *signal_semaphore_info, const VkSemaphoreSubmitInfo *wait_semaphore_info)
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

void vulkan_utils::changeImageLayout(const VkCommandBuffer command_buffer, const VkImage image, const VkImageLayout current_layout, const VkImageLayout new_layout)
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

void vulkan_utils::copyImage(const VkCommandBuffer command_buffer, const VkImage source, const VkImage destination, const VkExtent2D source_size, const VkExtent2D destination_size)
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

void VulkanEngine::initializeCommands()
{
    VkCommandPoolCreateInfo command_pool_info {
        vulkan_utils::generateCommandPoolCreateInfo(
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
            vulkan_utils::generateCommandBufferAllocateInfo(
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
        vulkan_utils::generateCommandBufferAllocateInfo(
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
        [this]
        {
            vkDestroyCommandPool(logical_device, immediate_command_pool, nullptr);

            for(auto& frame : frames)
            {
                vkDestroyCommandPool(logical_device, frame.command_pool, nullptr);
            }
        }
    );
}

void VulkanEngine::initializeSyncStructures()
{
    VkFenceCreateInfo fence_create_info {
        vulkan_utils::generateFenceCreateinfo(VK_FENCE_CREATE_SIGNALED_BIT)
    };

    VkSemaphoreCreateInfo semaphore_create_info {
        vulkan_utils::generateSemaphoreCreateInfo()
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

            for(auto& frame : frames)
            {
                vkDestroyFence(logical_device, frame.render_fence, nullptr);
                
                vkDestroySemaphore(logical_device, frame.render_semaphore, nullptr);
                vkDestroySemaphore(logical_device, frame.swapchain_semaphore, nullptr);
            }
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

    std::uint32_t swapchain_image_index;

    check(
        vkAcquireNextImageKHR(
            logical_device,
            swapchain,
            1'000'000'000,
            getCurrentFrame().swapchain_semaphore,
            nullptr,
            &swapchain_image_index
        )
    );

    check(
        vkResetFences(
            logical_device, 1, &getCurrentFrame().render_fence
        )
    );

    VkCommandBuffer command_buffer {getCurrentFrame().main_command_buffer};

    check(vkResetCommandBuffer(command_buffer, 0));

    VkCommandBufferBeginInfo command_buffer_begin_info {
        vulkan_utils::generateCommandBufferBeginInfo(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        )
    };

    draw_extent.width = draw_image.image_extent.width;
    draw_extent.height = draw_image.image_extent.height;    

    check(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

    vulkan_utils::changeImageLayout(
        command_buffer,
        draw_image.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL
    );

    drawBackground(command_buffer);

    vulkan_utils::changeImageLayout(
        command_buffer,
        draw_image.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    drawGeometry(command_buffer);

    vulkan_utils::changeImageLayout(
        command_buffer,
        draw_image.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );

    vulkan_utils::changeImageLayout(
        command_buffer,
        swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    vulkan_utils::copyImage(
        command_buffer,
        draw_image.image,
        swapchain_images[swapchain_image_index],
        draw_extent,
        swapchain_extent
    );

    vulkan_utils::changeImageLayout(
        command_buffer,
        swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );    

    check(
        vkEndCommandBuffer(command_buffer)
    );

    VkCommandBufferSubmitInfo command_buffer_info {
        vulkan_utils::generateCommandBufferSubmitInfo(command_buffer)
    };

    VkSemaphoreSubmitInfo wait_info {
        vulkan_utils::generateSemaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            getCurrentFrame().swapchain_semaphore
        )
    };

    VkSemaphoreSubmitInfo signal_info {
        vulkan_utils::generateSemaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            getCurrentFrame().render_semaphore
        )
    };

    VkSubmitInfo2 submit_info {
        vulkan_utils::generateSubmitInfo(
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

    check(
        vkQueuePresentKHR(graphics_queue, &present_info)
    );

    ++frame_number;
}

vulkan_utils::PipelineBuilder::PipelineBuilder()
{
    clear();
}

void vulkan_utils::PipelineBuilder::clear()
{
    input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };

    color_blend_attachment = {};

    multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };

    pipeline_layout = {};

    depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    render_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO
    };

    shader_stages.clear();
}

void vulkan_utils::PipelineBuilder::setShaders(const VkShaderModule vertex_shader, const VkShaderModule fragment_shader)
{
    shader_stages.clear();

    shader_stages.push_back(
        vulkan_utils::generatePipelineShaderStageCreateInfo(
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_shader
        )
    );

    shader_stages.push_back(
        vulkan_utils::generatePipelineShaderStageCreateInfo(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader
        )
    );    
}

void vulkan_utils::PipelineBuilder::setInputTopology(const VkPrimitiveTopology topology)
{
    input_assembly.topology = topology;

    input_assembly.primitiveRestartEnable = VK_FALSE;
}

void vulkan_utils::PipelineBuilder::setPolygonMode(const VkPolygonMode mode)
{
    rasterizer.polygonMode = mode;
    rasterizer.lineWidth = 1.f;
}

void vulkan_utils::PipelineBuilder::setCullMode(const VkCullModeFlags cull_mode, const VkFrontFace front_face)
{
    rasterizer.cullMode = cull_mode;
    rasterizer.frontFace = front_face;
}

void vulkan_utils::PipelineBuilder::setMultisamplingToNone()
{
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
}

void vulkan_utils::PipelineBuilder::disableBlending()
{
    color_blend_attachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_A_BIT   
    ;
    
    color_blend_attachment.blendEnable = VK_FALSE;
}

void vulkan_utils::PipelineBuilder::setColorAttachmentFormat(const VkFormat format)
{
    color_attachment_format = format;

    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachmentFormats = &color_attachment_format;
}

void vulkan_utils::PipelineBuilder::setDepthFormat(const VkFormat format)
{
    render_info.depthAttachmentFormat = format;
}

void vulkan_utils::PipelineBuilder::disableDepthTest()
{
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
}

VkPipeline vulkan_utils::PipelineBuilder::build(const VkDevice device)
{
    VkPipelineViewportStateCreateInfo viewport_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr
    };

    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blending {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr
    };

    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineVertexInputStateCreateInfo vertex_input_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 
    };

    VkGraphicsPipelineCreateInfo pipeline_info {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };

    pipeline_info.pNext = &render_info;

    pipeline_info.stageCount = static_cast<std::uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = pipeline_layout;

    VkDynamicState state[] {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };

    dynamic_info.pDynamicStates = state;
    dynamic_info.dynamicStateCount = 2;

    pipeline_info.pDynamicState = &dynamic_info;

    VkPipeline new_pipeline;

    if(
        vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &new_pipeline
        ) 
        != VK_SUCCESS
    )
    {
        throw std::runtime_error{"Failed to create pipeline!"};
    }
    
    return new_pipeline;
}

VkPipelineShaderStageCreateInfo vulkan_utils::generatePipelineShaderStageCreateInfo(const VkShaderStageFlagBits stage, const VkShaderModule shader_module, const std::string_view entry)
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

VkPipelineLayoutCreateInfo  vulkan_utils::generatePipelineLayoutCreateInfo()
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

bool vulkan_utils::loadShaderModule(const std::string_view file_path, const VkDevice device, VkShaderModule *const output_shader_module)
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

VkRenderingAttachmentInfo vulkan_utils::generateAttachmentInfo(const VkImageView view, const VkClearValue *const clear, const VkImageLayout layout)
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

VkRenderingInfo vulkan_utils::generateRenderingInfo(const VkExtent2D render_extent, const VkRenderingAttachmentInfo *const color_attachment, const VkRenderingAttachmentInfo *const depth_attachment)
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

void VulkanEngine::initializeTrianglePipeline()
{
    VkShaderModule triangle_fragment_shader;

    if(
        !vulkan_utils::loadShaderModule(
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
        !vulkan_utils::loadShaderModule(
            "../shaders/colored_triangle.vert.spv",
            logical_device,
            &triangle_vertex_shader
        )
    )
    {
        throw std::runtime_error{"Failed to load main vertex shader!"};
    }    

    VkPipelineLayoutCreateInfo pipeline_layout_info {
        vulkan_utils::generatePipelineLayoutCreateInfo()
    };

    check(
        vkCreatePipelineLayout(
            logical_device,
            &pipeline_layout_info,
            nullptr,
            &triangle_pipeline_layout
        )
    );

    vulkan_utils::PipelineBuilder pipeline_builder;

    pipeline_builder.pipeline_layout = triangle_pipeline_layout;
    
    pipeline_builder.setShaders(triangle_vertex_shader, triangle_fragment_shader);

    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

    pipeline_builder.setMultisamplingToNone();
    pipeline_builder.disableBlending();
    pipeline_builder.disableDepthTest();

    pipeline_builder.setColorAttachmentFormat(draw_image.image_format);
    pipeline_builder.setDepthFormat(VK_FORMAT_UNDEFINED);

    triangle_pipeline = pipeline_builder.build(logical_device);

    vkDestroyShaderModule(logical_device, triangle_fragment_shader, nullptr);
    vkDestroyShaderModule(logical_device, triangle_vertex_shader, nullptr);

    deletors.addDeletor(
        [this]
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
        !vulkan_utils::loadShaderModule(
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
        !vulkan_utils::loadShaderModule(
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
    buffer_range.size = sizeof(vulkan_utils::GPUDrawPushCostants);
    buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info {
        vulkan_utils::generatePipelineLayoutCreateInfo()
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

    vulkan_utils::PipelineBuilder pipeline_builder;

    pipeline_builder.pipeline_layout = mesh_pipeline_layout;
    
    pipeline_builder.setShaders(triangle_mesh_vertex_shader, triangle_fragment_shader);

    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

    pipeline_builder.setMultisamplingToNone();
    pipeline_builder.disableBlending();
    pipeline_builder.disableDepthTest();

    pipeline_builder.setColorAttachmentFormat(draw_image.image_format);
    pipeline_builder.setDepthFormat(VK_FORMAT_UNDEFINED);

    mesh_pipeline = pipeline_builder.build(logical_device);

    vkDestroyShaderModule(logical_device, triangle_fragment_shader, nullptr);
    vkDestroyShaderModule(logical_device, triangle_mesh_vertex_shader, nullptr);

    deletors.addDeletor(
        [this]
        {
            vkDestroyPipelineLayout(logical_device, mesh_pipeline_layout, nullptr);
            vkDestroyPipeline(logical_device, mesh_pipeline, nullptr);
        }
    );
}

void VulkanEngine::initializeDefaultData()
{
    std::array<vulkan_utils::Vertex, 4> rectangle_vertices;

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

    deletors.addDeletor(
        [this]
        {
            destroyBuffer(rectangle.index_buffer);
            destroyBuffer(rectangle.vertex_buffer);
        }
    );
}

void VulkanEngine::drawBackground(const VkCommandBuffer command_buffer)
{
    VkClearColorValue clear_value;

    clear_value = {
        {0.0f, 0.0f, 0.0f, 1.0f}
    };

    VkImageSubresourceRange clear_range {
        vulkan_utils::generateImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
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

void VulkanEngine::drawGeometry(const VkCommandBuffer command_buffer)
{
    VkRenderingAttachmentInfo color_attachment {
        vulkan_utils::generateAttachmentInfo(
            draw_image.image_view,
            nullptr,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        )
    };

    VkRenderingInfo render_info {
        vulkan_utils::generateRenderingInfo(
            draw_extent, &color_attachment, nullptr
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

    vulkan_utils::GPUDrawPushCostants push_constants;

    push_constants.world_matrix = glm::mat4{1.f};
    push_constants.vertex_buffer = rectangle.vertex_buffer_address;

    vkCmdPushConstants(
        command_buffer,
        mesh_pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(vulkan_utils::GPUDrawPushCostants),
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

void vulkan_utils::DeletionQueue::addDeletor(std::function<void()> &&deletor)
{
    deletors.emplace_back(std::move(deletor));
}

void vulkan_utils::DeletionQueue::flush()
{
    for(auto deletor_it {deletors.rbegin()}; deletor_it != deletors.rend(); ++deletor_it)
    {
        (*deletor_it)();
    }

    deletors.clear();
}

vulkan_utils::AllocatedBuffer VulkanEngine::createBuffer(const std::size_t allocate_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
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

    vulkan_utils::AllocatedBuffer new_buffer;

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

void VulkanEngine::destroyBuffer(const vulkan_utils::AllocatedBuffer &buffer)
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
        vulkan_utils::generateCommandBufferBeginInfo(
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
        vulkan_utils::generateCommandBufferSubmitInfo(immediate_command_buffer)
    };

    VkSubmitInfo2 submit_info {
        vulkan_utils::generateSubmitInfo(&command_buffer_submit_info, nullptr, nullptr)
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

vulkan_utils::GPUMeshBuffers VulkanEngine::uploadMesh(const std::span<std::uint32_t> indices, const std::span<vulkan_utils::Vertex> vertices)
{
    const std::size_t vertex_buffer_size {
        vertices.size() * sizeof(vulkan_utils::Vertex)
    };

    const std::size_t index_buffer_size {
        indices.size() * sizeof(std::uint32_t)
    };

    vulkan_utils::GPUMeshBuffers new_surface;

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

    vulkan_utils::AllocatedBuffer staging {
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
