#include "gpu/gpu_resources.h"

namespace gpu {

void GpuResources::reset() {
    static_quad_vertex_buffer_ = {};
    static_quad_index_buffer_ = {};
    instance_buffer_ = {};
    fog_texture_ = {};
    font_atlas_texture_ = {};
    text_vertex_buffer_ = VK_NULL_HANDLE;
    staged_instances_.clear();
    staged_fog_mask_.clear();
}

void GpuResources::initialize_placeholders() {
    static_quad_vertex_buffer_ = make_buffer_placeholder(sizeof(float) * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    static_quad_index_buffer_ = make_buffer_placeholder(sizeof(std::uint16_t) * 6, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    instance_buffer_ = make_buffer_placeholder(0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    fog_texture_ = make_texture_placeholder(0, 0, VK_FORMAT_R8_UNORM);
}

void GpuResources::bind_text_overlay_resources(
    VkImage font_atlas_image,
    VkImageView font_atlas_view,
    VkSampler font_atlas_sampler,
    VkBuffer text_vertex_buffer
) {
    font_atlas_texture_.handle = font_atlas_image;
    font_atlas_texture_.view = font_atlas_view;
    font_atlas_texture_.sampler = font_atlas_sampler;
    font_atlas_texture_.format = VK_FORMAT_R8G8B8A8_UNORM;
    text_vertex_buffer_ = text_vertex_buffer;
}

void GpuResources::upload_instance_data(std::span<const InstanceData> instances) {
    staged_instances_.assign(instances.begin(), instances.end());
    instance_buffer_.size_bytes = static_cast<VkDeviceSize>(staged_instances_.size() * sizeof(InstanceData));
}

void GpuResources::upload_fog_mask(std::span<const std::uint8_t> fog_mask, std::uint32_t width, std::uint32_t height) {
    staged_fog_mask_.assign(fog_mask.begin(), fog_mask.end());
    fog_texture_.width = width;
    fog_texture_.height = height;
    fog_texture_.format = VK_FORMAT_R8_UNORM;
}

} // namespace gpu
