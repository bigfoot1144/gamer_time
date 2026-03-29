#pragma once

#include "gpu/buffer_utils.h"
#include "gpu/texture_utils.h"
#include "render/render_world.h"

#include <cstdint>
#include <span>
#include <vector>

namespace gpu {

class GpuResources {
public:
    void reset();

    void initialize_placeholders();
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
    BufferAllocation static_quad_vertex_buffer_{};
    BufferAllocation static_quad_index_buffer_{};
    BufferAllocation instance_buffer_{};
    TextureAllocation fog_texture_{};
    TextureAllocation font_atlas_texture_{};
    VkBuffer text_vertex_buffer_ = VK_NULL_HANDLE;
    std::vector<InstanceData> staged_instances_;
    std::vector<std::uint8_t> staged_fog_mask_;
};

} // namespace gpu
