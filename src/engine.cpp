#include "engine.hpp"
#include "input_handler.hpp"
#include <SDL2/SDL_events.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

Engine* loaded_engine { nullptr };
Engine& Engine::Get() { return *loaded_engine; }

Engine::Engine() : _initialized            { false },
                   _stop_rendering         { false },
                   _frame_counter          { 0 },
                   _window_extent          { 2560, 1080 },
                   _swapchain_image_format { VK_FORMAT_B8G8R8A8_UNORM }
{
    #if DEBUG_LEVEL >= 1
    LOG("Engine instance created.", COMPONENT_NAME);
    #endif
}

Engine::Frame& Engine::current_frame()
{
    return _frames[_frame_counter % FRAME_OVERLAP];
};

void Engine::init()
{
    assert(loaded_engine == nullptr);
    loaded_engine = { this };

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags = { (SDL_WindowFlags)(SDL_WINDOW_VULKAN) };

    _window_ptr = { SDL_CreateWindow
                    (
                        "Dedalo Engine",
                        SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED,
                        _window_extent.width,
                        _window_extent.height,
                        window_flags
                    ) };

    _stopwatch.start();

    init_input_handler();
    init_vulkan();
    init_swapchain();
    init_images();
    init_commands();
    init_sync_structures();
    init_descriptor_sets();
    init_pipelines();

    _initialized = { true };

    #if DEBUG_LEVEL >= 1
    LOG("Engine initialized.", COMPONENT_NAME);
    //LOG("Stopwatch: " + _stopwatch.elapsed_as_string(), COMPONENT_NAME);
    #endif
}

std::string spv_direcory_path()
{
    std::filesystem::path spv_path = std::filesystem::current_path() / "shaders" / "spv";
    return spv_path.string() + "/";
}

void Engine::run()
{
    if (!_initialized)
        return;

    // main loop
    while (!_quit)
    {
        // handle events on queue
        _input_handler.handle_events();

        // do not draw if we are minimized
        if (_stop_rendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        //_stopwatch.start();
        draw();
        //LOG("Frametime: " + _stopwatch.elapsed_as_string(), COMPONENT_NAME);
    }
}

void Engine::cleanup()
{
    if (!_initialized)
        return;

    vkDeviceWaitIdle(_device_handle);

    _deletion_queue.flush(_device_handle);

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        vkDestroyCommandPool(_device_handle, _frames[i]._command_pool_handle, nullptr);
        vkDestroyFence(_device_handle, _frames[i]._render_fence_handle, nullptr);
        vkDestroySemaphore(_device_handle, _frames[i]._render_semaphore_handle, nullptr);
        vkDestroySemaphore(_device_handle, _frames[i]._swapchain_semaphore_handle, nullptr);

        _frames[i]._deletion_queue.flush();
    }

    _deletion_queue.flush();

    destroy_swapchain();
    vkDestroySurfaceKHR(_instance_handle, _surface_handle, nullptr);
    vkDestroyDevice(_device_handle, nullptr);
    vkb::destroy_debug_utils_messenger(_instance_handle, _debug_messenger_handle);
    vkDestroyInstance(_instance_handle, nullptr);

    SDL_DestroyWindow(_window_ptr);

    loaded_engine = { nullptr };

    #if DEBUG_LEVEL >= 1
    LOG("Engine destroyed.", COMPONENT_NAME);
    #endif
}

void Engine::quit()
{
    _quit = true;
}

void Engine::init_input_handler()
{
    _input_handler.add_binding<Engine>(SDL_QUIT, this, &Engine::quit);
}

void Engine::init_vulkan()
{
    vkb::InstanceBuilder builder {};

    #if DEBUG_LEVEL == 0
    vkb::Instance vkb_instance_handle { builder.set_app_name("dedalo_engine")
                                        .request_validation_layers(false)
                                        .use_default_debug_messenger()
                                        .require_api_version(1, 3, 0)
                                        .build()
                                        .value() };
    #endif

    #if DEBUG_LEVEL >= 1
    vkb::Instance vkb_instance_handle { builder.set_app_name("dedalo_engine")
                                        .request_validation_layers(true)
                                        .use_default_debug_messenger()
                                        .require_api_version(1, 3, 0)
                                        .build()
                                        .value() };
    #endif

    _instance_handle        = { vkb_instance_handle.instance };
    _debug_messenger_handle = { vkb_instance_handle.debug_messenger };

    SDL_Vulkan_CreateSurface(_window_ptr, _instance_handle, &_surface_handle);

    VkPhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = { true };
    features13.synchronization2 = { true };

    VkPhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = { true };
    features12.descriptorIndexing  = { true };

    vkb::PhysicalDeviceSelector physical_device_selector { vkb_instance_handle };
    vkb::PhysicalDevice         physical_device_selected { physical_device_selector
                                                            .set_minimum_version(1, 3)
                                                            .set_required_features_13(features13)
                                                            .set_required_features_12(features12)
                                                            .set_surface(_surface_handle)
                                                            .select()
                                                            .value() };

    vkb::DeviceBuilder device_builder { physical_device_selected };
    vkb::Device        device_builded { device_builder.build().value() };

    _device_handle          = { device_builded.device };
    _physical_device_handle = { physical_device_selected.physical_device };
    _graphics_queue_handle  = { device_builded.get_queue(vkb::QueueType::graphics).value() };
    _graphics_queue_family  = { device_builded.get_queue_index(vkb::QueueType::graphics).value() };

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.physicalDevice = { _physical_device_handle };
    allocator_create_info.device         = { _device_handle };
    allocator_create_info.instance       = { _instance_handle };
    allocator_create_info.flags          = { VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT };

    vmaCreateAllocator(&allocator_create_info, &_allocator);
    _deletion_queue.enqueue_deletor( [&]() { vmaDestroyAllocator(_allocator); } );
}

void Engine::init_swapchain()
{
    create_swapchain(_window_extent.width, _window_extent.height);
}

void Engine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchain_builder { _physical_device_handle, _device_handle, _surface_handle };
    vkb::Swapchain        swapchain_builded { swapchain_builder
                                                .set_desired_format(VkSurfaceFormatKHR { .format = _swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                                                .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                                .set_desired_extent(width, height)
                                                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                                .build()
                                                .value() };

    _swapchain_extent             = swapchain_builded.extent;
    _swapchain_handle             = swapchain_builded.swapchain;
    _swapchain_image_handles      = swapchain_builded.get_images().value();
    _swapchain_image_view_handles = swapchain_builded.get_image_views().value();
}

void Engine::destroy_swapchain()
{
    vkDestroySwapchainKHR(_device_handle, _swapchain_handle, nullptr);

    for (int i = 0; i < _swapchain_image_view_handles.size(); i++)
        vkDestroyImageView(_device_handle, _swapchain_image_view_handles[i], nullptr);
}

void Engine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo command_pool_info { vkinit::command_pool_create_info(_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) };

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        result_check(vkCreateCommandPool(_device_handle, &command_pool_info, nullptr, &_frames[i]._command_pool_handle));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmd_alloc_info { vkinit::command_buffer_allocate_info(_frames[i]._command_pool_handle, 1) };

        result_check(vkAllocateCommandBuffers(_device_handle, &cmd_alloc_info, &_frames[i]._command_buffer_handle));
    }
}

void Engine::init_sync_structures()
{
    // VkFenceCreateInfo fence_create_info { vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT) };
    VkSemaphoreCreateInfo semaphore_create_info { vkinit::semaphore_create_info() };

    VkFenceCreateInfo fence_create_info {};
    fence_create_info.sType = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_create_info.pNext = { nullptr };
    fence_create_info.flags = { VK_FENCE_CREATE_SIGNALED_BIT };

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        result_check(vkCreateFence(_device_handle, &fence_create_info, nullptr, &_frames[i]._render_fence_handle));
        result_check(vkCreateSemaphore(_device_handle, &semaphore_create_info, nullptr, &_frames[i]._swapchain_semaphore_handle));
        result_check(vkCreateSemaphore(_device_handle, &semaphore_create_info, nullptr, &_frames[i]._render_semaphore_handle));
    }
}

void Engine::init_images()
{
    VkExtent3D draw_image_extent { _window_extent.width, _window_extent.height, 1 };

    for (AllocatedImage& image : _images)
    {
        image._image_format = { VK_FORMAT_R32G32B32A32_SFLOAT };
        image._image_extent = { draw_image_extent };

        VkImageUsageFlags image_usages {};
        image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
        image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkImageCreateInfo image_create_info = vkinit::image_create_info(image._image_format, image_usages, image._image_extent);

        VmaAllocationCreateInfo image_alloc_info {};
        image_alloc_info.usage         = { VMA_MEMORY_USAGE_GPU_ONLY };
        image_alloc_info.requiredFlags = { VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
        vmaCreateImage(_allocator, &image_create_info, &image_alloc_info, &image._image_handle, &image._allocation, nullptr);

        VkImageViewCreateInfo imageview_create_info = vkinit::imageview_create_info(image._image_format, image._image_handle, VK_IMAGE_ASPECT_COLOR_BIT);
        result_check(vkCreateImageView(_device_handle, &imageview_create_info, nullptr, &image._image_view_handle));

        _deletion_queue.enqueue_deletor(
            [this, image](){
                vkDestroyImageView(_device_handle, image._image_view_handle, nullptr);
                vmaDestroyImage(_allocator, image._image_handle, image._allocation);
            }
        );
    }
}

void Engine::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size)
{
    VkImageBlit2 blit_region { .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
    blit_region.srcOffsets[1].x = src_size.width;
    blit_region.srcOffsets[1].y = src_size.height;
    blit_region.srcOffsets[1].z = 1;

    blit_region.dstOffsets[1].x = dst_size.width;
    blit_region.dstOffsets[1].y = dst_size.height;
    blit_region.dstOffsets[1].z = 1;

    blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount     = 1;
    blit_region.srcSubresource.mipLevel       = 0;

    blit_region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount     = 1;
    blit_region.dstSubresource.mipLevel       = 0;

    VkBlitImageInfo2 blit_info { .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
    blit_info.dstImage       = destination;
    blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit_info.srcImage       = source;
    blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_info.filter         = VK_FILTER_LINEAR;
    blit_info.regionCount    = 1;
    blit_info.pRegions       = &blit_region;

    vkCmdBlitImage2(cmd, &blit_info);
}

void Engine::transition_image_layout(VkCommandBuffer cmd_buff, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout)
{
    VkImageAspectFlags aspect_mask { (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT };

    VkImageMemoryBarrier2 image_barrier {};
    image_barrier.sType            = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    image_barrier.pNext            = { nullptr };
    image_barrier.srcStageMask     = { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT };
    image_barrier.srcAccessMask    = { VK_ACCESS_2_MEMORY_WRITE_BIT };
    image_barrier.dstStageMask     = { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT };
    image_barrier.dstAccessMask    = { VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT };
    image_barrier.oldLayout        = { current_layout };
    image_barrier.newLayout        = { new_layout };
    image_barrier.subresourceRange = { vkinit::image_subresource_range(aspect_mask) };
    image_barrier.image            = { image };

    VkDependencyInfo dep_info {};
    dep_info.sType                   = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep_info.pNext                   = { nullptr };
    dep_info.imageMemoryBarrierCount = { 1 };
    dep_info.pImageMemoryBarriers    = { &image_barrier };

    vkCmdPipelineBarrier2(cmd_buff, &dep_info);
}

bool Engine::load_shader_module(const std::filesystem::path& spv_path, VkShaderModule& shader_module)
{
    SpirVData shader { SpirVFileReader::readSpirVFile(spv_path) };

    VkShaderModuleCreateInfo create_info {};
    create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext    = nullptr;
    create_info.codeSize = shader.spv_code.size() * sizeof(uint32_t);
    create_info.pCode    = shader.spv_code.data();

    if (vkCreateShaderModule(_device_handle, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        return false;

    return true;
}

void Engine::draw()
{
    result_check
    (
        vkWaitForFences
        (
            _device_handle,
            1,
            &current_frame()._render_fence_handle,
            true,
            ONE_SECOND
        )
    );

    current_frame()._deletion_queue.flush();

    result_check
    (
        vkResetFences
        (
            _device_handle,
            1,
            &current_frame()._render_fence_handle
        )
    );

    uint32_t swapchain_image_index {};

    result_check
    (
        vkAcquireNextImageKHR
        (
            _device_handle,
            _swapchain_handle,
            ONE_SECOND,
            current_frame()._swapchain_semaphore_handle,
            nullptr,
            &swapchain_image_index
        )
    );

    VkCommandBuffer cmd_buff { current_frame()._command_buffer_handle };
    result_check(vkResetCommandBuffer(cmd_buff, 0));

    _draw_extent.width  = _images[0]._image_extent.width;
    _draw_extent.height = _images[0]._image_extent.height;

    VkCommandBufferBeginInfo cmd_buff_begin_info { vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) };
    result_check(vkBeginCommandBuffer(cmd_buff, &cmd_buff_begin_info));

    //////////

    transition_image_layout(cmd_buff, _images[2]._image_handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    if (_frame_counter == 0)
    {
        transition_image_layout(cmd_buff, _images[1]._image_handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transition_image_layout(cmd_buff, _images[0]._image_handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }

    else
    {
        transition_image_layout(cmd_buff, _images[1]._image_handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
        transition_image_layout(cmd_buff, _images[0]._image_handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
    }

    //////////

    compute_simulation_step(cmd_buff);

    //////////

    transition_image_layout(cmd_buff, _images[2]._image_handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image_layout(cmd_buff, _swapchain_image_handles[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_image_to_image(cmd_buff, _images[2]._image_handle, _swapchain_image_handles[swapchain_image_index], _draw_extent, _swapchain_extent);
    transition_image_layout(cmd_buff, _swapchain_image_handles[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //////////

    // Ho terminato di registrare i comandi nel command buffer;
    // a questo punto il command buffer è pronto per essere inviato alla GPU.
    result_check(vkEndCommandBuffer(cmd_buff));

    VkCommandBufferSubmitInfo cmd_buff_info { vkinit::command_buffer_submit_info(cmd_buff) };

    VkSemaphoreSubmitInfo wait_info {
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, current_frame()._swapchain_semaphore_handle)
    };

    VkSemaphoreSubmitInfo signal_info {
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, current_frame()._render_semaphore_handle)
    };

    // Invio del command buffer, del semaforo per la swapchain e del semaforo per il rendering
    VkSubmitInfo2 submit { vkinit::submit_info(&cmd_buff_info, &signal_info, &wait_info) };
    result_check(vkQueueSubmit2(_graphics_queue_handle, 1, &submit, current_frame()._render_fence_handle));

    VkPresentInfoKHR present_info {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.pSwapchains        = &_swapchain_handle;
    present_info.swapchainCount     = 1;
    present_info.pWaitSemaphores    = &current_frame()._render_semaphore_handle;
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices      = &swapchain_image_index;

    result_check(vkQueuePresentKHR(_graphics_queue_handle, &present_info));

    _frame_counter++;
}

void Engine::init_pipelines()
{
    init_compute_pipeline(_advection_pipeline_handle, _advection_pipeline_layout_handle, spv_direcory_path() + "advection.comp.spv");
    init_compute_pipeline(_swap_pipeline_handle, _swap_pipeline_layout_handle, spv_direcory_path() + "swap.comp.spv");
    init_compute_pipeline(_jacobi_diffusion_pipeline_handle, _jacobi_diffusion_pipeline_layout_handle, spv_direcory_path() + "jacobi_diffusion.comp.spv");
    init_compute_pipeline(_jacobi_pression_pipeline_handle, _jacobi_pression_pipeline_layout_handle, spv_direcory_path() + "jacobi_pression.comp.spv");
    init_compute_pipeline(_remove_divergency_pipeline_handle, _remove_divergency_pipeline_layout_handle, spv_direcory_path() + "remove_divergency.comp.spv");
}

void Engine::init_compute_pipeline(VkPipeline& pipeline_handle, VkPipelineLayout& pipeline_layout_handle, const std::string& spv_path)
{
    VkPushConstantRange push_constant_range {};
    push_constant_range.offset     = 0;
    push_constant_range.size       = sizeof(ComputePushConstants);
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
    pipeline_layout_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext                  = nullptr;
    pipeline_layout_create_info.pSetLayouts            = &_descriptor_set_layout_handle;
    pipeline_layout_create_info.setLayoutCount         = 1;
    pipeline_layout_create_info.pPushConstantRanges    = &push_constant_range;
    pipeline_layout_create_info.pushConstantRangeCount = 1;

    result_check(vkCreatePipelineLayout(_device_handle, &pipeline_layout_create_info, nullptr, &pipeline_layout_handle));

    VkShaderModule shader_module {};

    if (!load_shader_module(spv_path, shader_module))
    {
        #if DEBUG_LEVEL >= 1
        LOG("Error when building the compute shader.", COMPONENT_NAME);
        #endif
    }

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info {};
    pipeline_shader_stage_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_info.pNext  = nullptr;
    pipeline_shader_stage_create_info.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_shader_stage_create_info.module = shader_module;
    pipeline_shader_stage_create_info.pName  = "main";

    VkComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.pNext  = nullptr;
    compute_pipeline_create_info.layout = pipeline_layout_handle;
    compute_pipeline_create_info.stage  = pipeline_shader_stage_create_info;

    result_check(vkCreateComputePipelines(_device_handle, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &pipeline_handle));

    vkDestroyShaderModule(_device_handle, shader_module, nullptr);

    _deletion_queue.enqueue_deletor(
        [&]() {
            vkDestroyPipelineLayout(_device_handle, pipeline_layout_handle, nullptr);
            vkDestroyPipeline(_device_handle, pipeline_handle, nullptr);
        }
    );
}

void Engine::init_descriptor_sets()
{
    size_t image_count { _images.size() };

    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, static_cast<float>(image_count * 2)  }
    };

    _global_descriptor_allocator.init_pool(_device_handle, 10, sizes);

    DescriptorLayoutBuilder layout_builder {};

    for (uint32_t i {}; i < image_count; ++i)
    {
        layout_builder.add_binding(i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    _descriptor_set_layout_handle = layout_builder.build(_device_handle, VK_SHADER_STAGE_COMPUTE_BIT);
    _descriptor_set_0_handle      = _global_descriptor_allocator.allocate(_device_handle, _descriptor_set_layout_handle);
    _descriptor_set_1_handle      = _global_descriptor_allocator.allocate(_device_handle, _descriptor_set_layout_handle);

    VkDescriptorImageInfo desc_image_0_info {};
    desc_image_0_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    desc_image_0_info.imageView   = _images[0]._image_view_handle;

    VkDescriptorImageInfo desc_image_1_info {};
    desc_image_1_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    desc_image_1_info.imageView   = _images[1]._image_view_handle;

    VkDescriptorImageInfo desc_image_2_info {};
    desc_image_2_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    desc_image_2_info.imageView   = _images[2]._image_view_handle;

    // Configura il primo DescriptorSet (image_0 -> input, image_1 -> output)
    VkWriteDescriptorSet descriptor_writes_0[3] {};

    descriptor_writes_0[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_0[0].dstBinding      = 0;
    descriptor_writes_0[0].dstSet          = _descriptor_set_0_handle;
    descriptor_writes_0[0].descriptorCount = 1;
    descriptor_writes_0[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_0[0].pImageInfo      = &desc_image_0_info;

    descriptor_writes_0[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_0[1].dstBinding      = 1;
    descriptor_writes_0[1].dstSet          = _descriptor_set_0_handle;
    descriptor_writes_0[1].descriptorCount = 1;
    descriptor_writes_0[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_0[1].pImageInfo      = &desc_image_1_info;

    descriptor_writes_0[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_0[2].dstBinding      = 2;
    descriptor_writes_0[2].dstSet          = _descriptor_set_0_handle;
    descriptor_writes_0[2].descriptorCount = 1;
    descriptor_writes_0[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_0[2].pImageInfo      = &desc_image_2_info;

    vkUpdateDescriptorSets(_device_handle, 3, descriptor_writes_0, 0, nullptr);

    // Configura il secondo DescriptorSet (image_1 -> input, image_0 -> output)
    VkWriteDescriptorSet descriptor_writes_1[3] {};

    descriptor_writes_1[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_1[0].dstBinding      = 0;
    descriptor_writes_1[0].dstSet          = _descriptor_set_1_handle;
    descriptor_writes_1[0].descriptorCount = 1;
    descriptor_writes_1[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_1[0].pImageInfo      = &desc_image_1_info;

    descriptor_writes_1[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_1[1].dstBinding      = 1;
    descriptor_writes_1[1].dstSet          = _descriptor_set_1_handle;
    descriptor_writes_1[1].descriptorCount = 1;
    descriptor_writes_1[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_1[1].pImageInfo      = &desc_image_0_info;

    descriptor_writes_1[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes_1[2].dstBinding      = 2;
    descriptor_writes_1[2].dstSet          = _descriptor_set_1_handle;
    descriptor_writes_1[2].descriptorCount = 1;
    descriptor_writes_1[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes_1[2].pImageInfo      = &desc_image_2_info;

    vkUpdateDescriptorSets(_device_handle, 3, descriptor_writes_1, 0, nullptr);

    _deletion_queue.enqueue_deletor(
        [&](){
            _global_descriptor_allocator.destroy_pool(_device_handle);
            vkDestroyDescriptorSetLayout(_device_handle, _descriptor_set_layout_handle, nullptr);
        }
    );
}

void Engine::run_jacobi_solver( VkCommandBuffer cmd_buff,
                                VkPipeline jacobi_pipeline_handle,
                                VkPipelineLayout jacobi_pipeline_layout_handle,
                                const ComputePushConstants& pc,
                                int iterations )
{
    // Variabili per alternare tra image0 e image1
    bool toggle { false };

    for (int i = 0; i < iterations; ++i)
    {
        vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, jacobi_pipeline_handle);

        if (toggle)
            vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, jacobi_pipeline_layout_handle, 0, 1, &_descriptor_set_0_handle, 0, nullptr);
        else
            vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, jacobi_pipeline_layout_handle, 0, 1, &_descriptor_set_1_handle, 0, nullptr);

        vkCmdPushConstants(cmd_buff, jacobi_pipeline_layout_handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);
        vkCmdDispatch(cmd_buff, std::ceil(_draw_extent.width / 16.0), std::ceil(_draw_extent.height / 16.0), 1);

        // Assicura che tutte le operazioni di scrittura siano completate prima della prossima iterazione
        VkMemoryBarrier memory_barrier {};
        memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

        // Alterna tra imgA e imgB
        toggle = !toggle;
    }
}

void Engine::dispatch_compute(VkCommandBuffer cmd_buff, VkPipeline pipeline_handle, VkPipelineLayout pipeline_layout_handle, const ComputePushConstants& pc)
{
    vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_handle);
    vkCmdBindDescriptorSets(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_handle, 0, 1, &_descriptor_set_0_handle, 0, nullptr);
    vkCmdPushConstants(cmd_buff, pipeline_layout_handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);
    vkCmdDispatch(cmd_buff, std::ceil(_draw_extent.width / 16.0), std::ceil(_draw_extent.height / 16.0), 1);
}

void Engine::compute_simulation_step(VkCommandBuffer cmd_buff)
{
    ComputePushConstants pc {};
    pc.mouse_down   = _input_handler.mouse_down;
    pc.time_elapsed = (uint32_t) _stopwatch.elapsed();
    pc.mouse_pos    = glm::ivec2(_input_handler.mouse_x, _input_handler.mouse_y);

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    VkMemoryBarrier memory_barrier {};
    memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    //LOG("Delta time: " + _stopwatch.elapsed_as_string(), COMPONENT_NAME);

    #define NUM_ITER 20

    // diffusion
    // pression
    // remove divergency
    // advection
    // pression
    // rempove divergency

    // Diffusion pass
    run_jacobi_solver(cmd_buff, _jacobi_diffusion_pipeline_handle, _jacobi_diffusion_pipeline_layout_handle, pc, NUM_ITER );

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Pression pass
    run_jacobi_solver(cmd_buff, _jacobi_pression_pipeline_handle, _jacobi_pression_pipeline_layout_handle, pc, NUM_ITER );

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Remove divergence pass
    dispatch_compute(cmd_buff, _remove_divergency_pipeline_handle, _remove_divergency_pipeline_layout_handle, pc);

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Advection pass
    dispatch_compute(cmd_buff, _advection_pipeline_handle, _advection_pipeline_layout_handle, pc);

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Swap pass
    dispatch_compute(cmd_buff, _swap_pipeline_handle, _swap_pipeline_layout_handle, pc);

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Pression pass
    run_jacobi_solver(cmd_buff, _jacobi_pression_pipeline_handle, _jacobi_pression_pipeline_layout_handle, pc, NUM_ITER );

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    // Remove divergence pass
    dispatch_compute(cmd_buff, _remove_divergency_pipeline_handle, _remove_divergency_pipeline_layout_handle, pc);

    // Assicura che tutte le operazioni di scrittura siano completate prima del prossimo step
    vkCmdPipelineBarrier(cmd_buff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);

    _stopwatch.start();
}
















