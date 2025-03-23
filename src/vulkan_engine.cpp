#include "vulkan_engine.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vulkan/vk_enum_string_helper.h>

#include <iostream>
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

    deletors.push_back(
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

    deletors.push_back(
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

    deletors.push_back(
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

    deletors.push_back(
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

    deletors.push_back(
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

VkImageCreateInfo VulkanEngine::generateImageCreateInfo(
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

VkImageViewCreateInfo VulkanEngine::generateImageViewCreateInfo(
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

    deletors.push_back(
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

    for(auto deletor_it {deletors.rbegin()}; deletor_it != deletors.rend(); ++deletor_it)
    {
        (*deletor_it)();
    } 

    deletors.clear();
}

void VulkanEngine::check(const VkResult result)
{
    if(result)
    {
        std::println("Detected Vulkan error: {}.", string_VkResult(result));
        std::abort();
    }
}

FrameData& VulkanEngine::getCurrentFrame()
{
    return frames[frame_number % frame_overlap];
}

VkCommandPoolCreateInfo VulkanEngine::generateCommandPoolCreateInfo(const std::uint32_t queue_family_index, const VkCommandPoolCreateFlags flags)
{
    VkCommandPoolCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr
    };

    info.queueFamilyIndex = queue_family_index;
    info.flags = flags;

    return info;
}

VkCommandBufferAllocateInfo VulkanEngine::generateCommandBufferAllocateInfo(const VkCommandPool pool, const std::uint32_t count)
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

VkFenceCreateInfo VulkanEngine::generateFenceCreateinfo(const VkFenceCreateFlags flags)
{
    VkFenceCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr
    };

    info.flags = flags;

    return info;
}

VkSemaphoreCreateInfo VulkanEngine::generateSemaphoreCreateInfo(const VkSemaphoreCreateFlags flags)
{
    VkSemaphoreCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr
    };

    info.flags = flags;

    return info;
}

VkCommandBufferBeginInfo VulkanEngine::generateCommandBufferBeginInfo(const VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr
    };

    info.pInheritanceInfo = nullptr;
    
    info.flags = flags;

    return info;
}

VkImageSubresourceRange VulkanEngine::generateImageSubresourceRange(const VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange sub_image {};

    sub_image.aspectMask = aspect_mask;
    sub_image.baseMipLevel = 0;
    sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
    sub_image.baseArrayLayer = 0;
    sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return sub_image;
}

VkSemaphoreSubmitInfo VulkanEngine::generateSemaphoreSubmitInfo(const VkPipelineStageFlags2 stage_mask, const VkSemaphore semaphore)
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

VkCommandBufferSubmitInfo VulkanEngine::generateCommandBufferSubmitInfo(const VkCommandBuffer command_buffer)
{
    VkCommandBufferSubmitInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr
    };

    info.commandBuffer = command_buffer;
    info.deviceMask = 0;

    return info;
}

VkSubmitInfo2 VulkanEngine::generateSubmitInfo(const VkCommandBufferSubmitInfo *command_buffer, const VkSemaphoreSubmitInfo *signal_semaphore_info, const VkSemaphoreSubmitInfo *wait_semaphore_info)
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

void VulkanEngine::changeImageLayout(const VkCommandBuffer command_buffer, const VkImage image, const VkImageLayout current_layout, const VkImageLayout new_layout)
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

    deletors.push_back(
        [this]
        {
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

    deletors.push_back(
        [this]
        {
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

    check(
        vkResetFences(
            logical_device, 1, &getCurrentFrame().render_fence
        )
    );

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
        swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL
    );

    // DRAW ON SWAPCHAIN IMAGE

    VkClearColorValue clear_value;

    const float flash {std::abs(std::sin(frame_number / 120.f))};

    clear_value = {
        {0.0f, 0.0f, flash, 1.0f}
    };
    
    VkImageSubresourceRange clear_range {
        generateImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
    };

    vkCmdClearColorImage(
        command_buffer,
        swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_GENERAL,
        &clear_value,
        1,
        &clear_range
    );

    //

    changeImageLayout(
        command_buffer,
        swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_GENERAL,
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

    check(
        vkQueuePresentKHR(graphics_queue, &present_info)
    );

    ++frame_number;
}
