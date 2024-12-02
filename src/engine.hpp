#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

#include "VkBootstrap.h"
#include "vk_initializers.h"
#include "vk_mem_alloc.h"

#include "deletion_queue.hpp"
#include "descriptor_allocator.hpp"
#include "descriptor_layout_builder.hpp"
#include "result_check.hpp"
#include "spirv_data.hpp"
#include "spirv_file_reader.hpp"
#include "logger.hpp"
#include "stopwatch.hpp"
#include "input_handler.hpp"

#define DEBUG_LEVEL 1

constexpr uint64_t ONE_SECOND = 1000000000;
constexpr unsigned int FRAME_OVERLAP = 2;

class Engine {
public:
    static constexpr std::string COMPONENT_NAME { "ENGINE" };

    Engine();
    static Engine& Get();

    void init();
    void run();
    void cleanup();

private:
    bool _initialized    {};
    bool _stop_rendering {};
    int  _frame_counter  {};
    bool _quit           {};

    Stopwatch    _stopwatch     {};
    InputHandler _input_handler {};

    struct SDL_Window* _window_ptr {};

    DeletionQueue _deletion_queue {};

    VkInstance               _instance_handle         {};
    VkDebugUtilsMessengerEXT _debug_messenger_handle  {};
    VkPhysicalDevice         _physical_device_handle  {};
    VkDevice                 _device_handle           {};
    VkSurfaceKHR             _surface_handle          {};
    VkExtent2D               _swapchain_extent        {};
    VkExtent2D               _window_extent           {};

    VkSwapchainKHR           _swapchain_handle             {};
    std::vector<VkImage>     _swapchain_image_handles      {};
    std::vector<VkImageView> _swapchain_image_view_handles {};
    VkFormat                 _swapchain_image_format       {};

    VkQueue  _graphics_queue_handle {};
    uint32_t _graphics_queue_family {};

    VkDescriptorSet       _descriptor_set_0_handle      {};
    VkDescriptorSet       _descriptor_set_1_handle      {};
    VkDescriptorSetLayout _descriptor_set_layout_handle {};
    DescriptorAllocator   _global_descriptor_allocator  {};

    VmaAllocator _allocator {};

    VkPipeline       _pipeline_handle        {};
    VkPipelineLayout _pipeline_layout_handle {};

    struct AllocatedImage
    {
        VkImage       _image_handle      {};
        VkImageView   _image_view_handle {};
        VmaAllocation _allocation        {};
        VkExtent3D    _image_extent      {};
        VkFormat      _image_format      {};
    };

    std::vector<AllocatedImage> _images { 3 };
    VkExtent2D _draw_extent {};

    struct Frame
    {
        VkCommandPool   _command_pool_handle        {};
        VkCommandBuffer _command_buffer_handle      {};
        VkSemaphore     _swapchain_semaphore_handle {};
        VkSemaphore     _render_semaphore_handle    {};
        VkFence         _render_fence_handle        {};
        DeletionQueue   _deletion_queue             {};
    };

    Frame _frames[FRAME_OVERLAP];

    struct ComputePushConstants
    {
        uint32_t mouse_down   {};
        uint32_t time_elapsed {};
        glm::ivec2 mouse_pos  {};
    };

    ////////

    Frame& current_frame();

    void init_input_handler();
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptor_sets();
    void init_images();
    void init_pipelines();

    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    void copy_image_to_image
    (
        VkCommandBuffer cmd,
        VkImage         source,
        VkImage         destination,
        VkExtent2D      src_size,
        VkExtent2D      dst_size
    );

    void transition_image_layout
    (
        VkCommandBuffer cmd_buff,
        VkImage         image,
        VkImageLayout   current_layout,
        VkImageLayout   new_layout
    );

    bool load_shader_module(const std::filesystem::path& spv_path, VkShaderModule& shader_module);
    std::string spv_directory_path();

    void draw();
    void quit();

    // fluid stuff

    VkPipeline       _jacobi_diffusion_pipeline_handle        {};
    VkPipelineLayout _jacobi_diffusion_pipeline_layout_handle {};

    VkPipeline       _jacobi_pression_pipeline_handle        {};
    VkPipelineLayout _jacobi_pression_pipeline_layout_handle {};

    VkPipeline       _advection_pipeline_handle        {};
    VkPipelineLayout _advection_pipeline_layout_handle {};

    VkPipeline       _remove_divergency_pipeline_handle        {};
    VkPipelineLayout _remove_divergency_pipeline_layout_handle {};

    VkPipeline       _swap_pipeline_handle        {};
    VkPipelineLayout _swap_pipeline_layout_handle {};

    void init_compute_pipeline(VkPipeline& pipeline_handle, VkPipelineLayout& pipeline_layout_handle, const std::string& spv_path);
    void dispatch_compute(VkCommandBuffer cmd_buff, VkPipeline pipeline_handle, VkPipelineLayout pipeline_layout_handle, const ComputePushConstants& pc);
    void compute_simulation_step(VkCommandBuffer cmd_buff);

    void run_jacobi_solver( VkCommandBuffer cmd_buff,
                            VkPipeline jacobi_pipeline_handle,
                            VkPipelineLayout jacobi_pipeline_layout_handle,
                            const ComputePushConstants& pc,
                            int iterations );
};
