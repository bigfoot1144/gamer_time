#pragma once

#include "common.h"
#include "gpu/swapchain_manager.h"
#include "gpu/vulkan_context.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void init(SDL_Window * window, const std::string & shader_dir, gpu::VulkanContext & context, gpu::SwapchainManager & swapchain);
    void shutdown();

    void on_window_resized();
    void request_resize() { framebuffer_resized_ = true; }
    void set_overlay_text(std::string text);
    bool begin_frame(uint32_t & image_index);
    void record_frame(uint32_t image_index);
    void submit_frame(uint32_t image_index);
    void draw_frame();
    void wait_idle();

private:
    struct TextVertex {
        float position[2];
        float uv[2];
    };

    SDL_Window * window_ = nullptr;
    gpu::VulkanContext * context_ = nullptr;
    gpu::SwapchainManager * swapchain_ = nullptr;
    std::string shader_dir_;
    std::string overlay_text_;
    bool framebuffer_resized_ = false;
    bool text_dirty_ = true;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout text_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline text_pipeline_ = VK_NULL_HANDLE;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;

    VkDescriptorSetLayout text_descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool text_descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet text_descriptor_set_ = VK_NULL_HANDLE;

    VkImage font_atlas_image_ = VK_NULL_HANDLE;
    VkDeviceMemory font_atlas_image_memory_ = VK_NULL_HANDLE;
    VkImageView font_atlas_image_view_ = VK_NULL_HANDLE;
    VkSampler font_atlas_sampler_ = VK_NULL_HANDLE;

    VkBuffer text_vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory text_vertex_buffer_memory_ = VK_NULL_HANDLE;
    std::size_t text_vertex_capacity_ = 0;
    std::uint32_t text_vertex_count_ = 0;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    size_t current_frame_ = 0;
    bool initialized_ = false;

    void create_render_pass();
    void create_graphics_pipeline();
    void create_text_descriptor_set_layout();
    void create_text_resources();
    void create_text_pipeline();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain_dependent_state();
    void recreate_swapchain();

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    void create_buffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer & buffer,
        VkDeviceMemory & buffer_memory
    ) const;
    void create_image(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage & image,
        VkDeviceMemory & image_memory
    ) const;
    VkImageView create_image_view(VkImage image, VkFormat format) const;
    VkCommandBuffer begin_single_time_commands() const;
    void end_single_time_commands(VkCommandBuffer command_buffer) const;
    void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) const;
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    void update_text_vertices();

    std::vector<uint32_t> load_spirv_file(const std::string & path) const;
    VkShaderModule create_shader_module(const std::vector<uint32_t> & code) const;
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);

    VkDevice device() const;
    VkPhysicalDevice physical_device() const;
    VkQueue graphics_queue() const;
    VkQueue present_queue() const;
    const gpu::SwapchainManager & swapchain_state() const;
    gpu::SwapchainManager & swapchain_state();
};
