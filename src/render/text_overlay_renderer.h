#pragma once

#include "gpu/gpu_resources.h"
#include "gpu/swapchain_manager.h"
#include "gpu/vulkan_context.h"

#include <string>
#include <vector>

class TextOverlayRenderer {
public:
    void initialize(
        gpu::VulkanContext & context,
        gpu::SwapchainManager & swapchain,
        gpu::GpuResources & resources,
        const std::string & shader_dir,
        VkRenderPass render_pass
    );
    void shutdown();
    void on_render_pass_changed(VkRenderPass render_pass);
    void set_text(std::string text);
    void prepare_frame();
    void record(VkCommandBuffer command_buffer) const;

    const std::string & text() const {
        return text_;
    }

private:
    struct TextVertex {
        float position[2];
        float uv[2];
    };

    gpu::VulkanContext * context_ = nullptr;
    gpu::SwapchainManager * swapchain_ = nullptr;
    gpu::GpuResources * resources_ = nullptr;
    std::string shader_dir_;
    std::string text_;
    bool dirty_ = true;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
    VkImage font_atlas_image_ = VK_NULL_HANDLE;
    VkDeviceMemory font_atlas_image_memory_ = VK_NULL_HANDLE;
    VkImageView font_atlas_image_view_ = VK_NULL_HANDLE;
    VkSampler font_atlas_sampler_ = VK_NULL_HANDLE;
    VkBuffer text_vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory text_vertex_buffer_memory_ = VK_NULL_HANDLE;
    std::size_t text_vertex_capacity_ = 0;
    std::uint32_t text_vertex_count_ = 0;

    void create_descriptor_set_layout();
    void create_font_resources();
    void create_pipeline();
    void destroy_pipeline_objects();
    void destroy_resource_objects();
    uint32_t glyph_index_for_byte(unsigned char c, bool & is_blank) const;
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
    void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) const;
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    std::vector<uint32_t> load_spirv_file(const std::string & path) const;
    VkShaderModule create_shader_module(const std::vector<uint32_t> & code) const;
    VkDevice device() const;
    VkPhysicalDevice physical_device() const;
    VkQueue graphics_queue() const;
};
