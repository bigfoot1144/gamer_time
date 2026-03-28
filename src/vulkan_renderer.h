#pragma once

#include "common.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool complete() const {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void init(SDL_Window* window, const std::string& shader_dir);
    void shutdown();

    void on_window_resized();
    void request_resize() { framebuffer_resized_ = true; }
    void set_overlay_text(std::string text);
    void draw_frame();
    void wait_idle();

private:
    struct TextVertex {
        float position[2];
        float uv[2];
    };

    SDL_Window* window_ = nullptr;
    std::string shader_dir_;
    std::string overlay_text_;
    bool framebuffer_resized_ = false;
    bool text_dirty_ = true;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_image_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout text_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline text_pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

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

    void create_instance();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_text_descriptor_set_layout();
    void create_text_resources();
    void create_text_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain();
    void recreate_swapchain();

    QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;
    bool check_device_extension_support(VkPhysicalDevice device) const;
    SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device) const;
    bool is_device_suitable(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const;
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    void create_buffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& buffer_memory
    ) const;
    void create_image(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& image_memory
    ) const;
    VkImageView create_image_view(VkImage image, VkFormat format) const;
    VkCommandBuffer begin_single_time_commands() const;
    void end_single_time_commands(VkCommandBuffer command_buffer) const;
    void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) const;
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    void update_text_vertices();

    std::vector<uint32_t> load_spirv_file(const std::string& path) const;
    VkShaderModule create_shader_module(const std::vector<uint32_t>& code) const;
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);
};
