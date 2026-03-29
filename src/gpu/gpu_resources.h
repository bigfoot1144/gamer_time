#pragma once

#include "gpu/buffer_utils.h"
#include "gpu/texture_utils.h"
#include "gpu/vulkan_context.h"
#include "render/render_world.h"

#include <cstdint>
#include <span>
#include <vector>

namespace gpu {

class GpuResources {
public:
    void initialize(VulkanContext & context);
    void shutdown();
    void reset();

    void bind_text_overlay_resources(
        VkImage font_atlas_image,
        VkImageView font_atlas_view,
        VkSampler font_atlas_sampler,
        VkBuffer text_vertex_buffer
    );

    void upload_instance_data(std::span<const InstanceData> instances);
    void upload_fog_mask(std::span<const std::uint8_t> fog_mask, std::uint32_t width, std::uint32_t height);

    const BufferAllocation & static_quad_vertex_buffer() const { return static_quad_vertex_buffer_; }
    const BufferAllocation & static_quad_index_buffer() const { return static_quad_index_buffer_; }
    const BufferAllocation & instance_buffer() const { return instance_buffer_; }
    const TextureAllocation & fog_texture() const { return fog_texture_; }
    const TextureAllocation & font_atlas_texture() const { return font_atlas_texture_; }
    VkBuffer text_vertex_buffer() const { return text_vertex_buffer_; }

    std::span<const InstanceData> staged_instances() const { return staged_instances_; }
    std::span<const std::uint8_t> staged_fog_mask() const { return staged_fog_mask_; }

private:
    VulkanContext * context_ = nullptr;
    VkCommandPool upload_command_pool_ = VK_NULL_HANDLE;
    BufferAllocation static_quad_vertex_buffer_{};
    BufferAllocation static_quad_index_buffer_{};
    BufferAllocation instance_buffer_{};
    TextureAllocation fog_texture_{};
    TextureAllocation font_atlas_texture_{};
    VkBuffer text_vertex_buffer_ = VK_NULL_HANDLE;
    std::vector<InstanceData> staged_instances_;
    std::vector<std::uint8_t> staged_fog_mask_;

    void create_upload_command_pool();
    void create_static_quad_buffers();
    void ensure_instance_buffer_capacity(VkDeviceSize required_size);
    void ensure_fog_texture(std::uint32_t width, std::uint32_t height);
    void destroy_buffer(BufferAllocation & allocation);
    void destroy_texture(TextureAllocation & allocation);
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    BufferAllocation create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) const;
    TextureAllocation create_texture(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties
    ) const;
    VkImageView create_image_view(VkImage image, VkFormat format) const;
    VkCommandBuffer begin_single_time_commands() const;
    void end_single_time_commands(VkCommandBuffer command_buffer) const;
    void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) const;
    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const;
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height) const;
    VkDevice device() const;
    VkPhysicalDevice physical_device() const;
    VkQueue graphics_queue() const;
};

} // namespace gpu
