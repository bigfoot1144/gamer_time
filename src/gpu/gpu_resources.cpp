#include "gpu/gpu_resources.h"

#include <array>
#include <cstring>

namespace gpu {

namespace {

constexpr VkDeviceSize kMinimumInstanceBufferBytes = sizeof(InstanceData) * 16;

}

void GpuResources::initialize(VulkanContext & context) {
    shutdown();
    context_ = &context;
    create_upload_command_pool();
    create_static_quad_buffers();
    ensure_instance_buffer_capacity(kMinimumInstanceBufferBytes);
}

void GpuResources::shutdown() {
    if (context_ != nullptr && device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device());
        destroy_buffer(instance_buffer_);
        destroy_buffer(static_quad_index_buffer_);
        destroy_buffer(static_quad_vertex_buffer_);
        destroy_texture(fog_texture_);
        destroy_texture(scene_atlas_texture_);
        if (upload_command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device(), upload_command_pool_, nullptr);
            upload_command_pool_ = VK_NULL_HANDLE;
        }
    }

    reset();
}

void GpuResources::reset() {
    context_ = nullptr;
    upload_command_pool_ = VK_NULL_HANDLE;
    static_quad_vertex_buffer_ = {};
    static_quad_index_buffer_ = {};
    instance_buffer_ = {};
    fog_texture_ = {};
    scene_atlas_texture_ = {};
    font_atlas_texture_ = {};
    text_vertex_buffer_ = VK_NULL_HANDLE;
    staged_instances_.clear();
    staged_fog_mask_.clear();
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

    const VkDeviceSize required_size = staged_instances_.empty()
        ? kMinimumInstanceBufferBytes
        : static_cast<VkDeviceSize>(staged_instances_.size() * sizeof(InstanceData));
    ensure_instance_buffer_capacity(required_size);
    instance_buffer_.size_bytes = required_size;

    if (staged_instances_.empty()) {
        return;
    }

    void * mapped_data = nullptr;
    check_vk(vkMapMemory(device(), instance_buffer_.memory, 0, required_size, 0, &mapped_data), "Failed to map instance buffer");
    std::memcpy(mapped_data, staged_instances_.data(), static_cast<size_t>(required_size));
    vkUnmapMemory(device(), instance_buffer_.memory);
}

void GpuResources::upload_fog_mask(std::span<const std::uint8_t> fog_mask, std::uint32_t width, std::uint32_t height) {
    staged_fog_mask_.assign(fog_mask.begin(), fog_mask.end());
    if (width == 0 || height == 0 || staged_fog_mask_.empty()) {
        return;
    }

    ensure_fog_texture(width, height);
    fog_texture_.width = width;
    fog_texture_.height = height;
    fog_texture_.format = VK_FORMAT_R8_UNORM;

    BufferAllocation staging_buffer = create_buffer(
        staged_fog_mask_.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void * mapped_data = nullptr;
    check_vk(vkMapMemory(device(), staging_buffer.memory, 0, staged_fog_mask_.size(), 0, &mapped_data), "Failed to map fog staging buffer");
    std::memcpy(mapped_data, staged_fog_mask_.data(), staged_fog_mask_.size());
    vkUnmapMemory(device(), staging_buffer.memory);

    transition_image_layout(fog_texture_.handle, fog_texture_.format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer.handle, fog_texture_.handle, width, height);
    transition_image_layout(fog_texture_.handle, fog_texture_.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    destroy_buffer(staging_buffer);
}

void GpuResources::upload_scene_atlas(const LoadedImage & image) {
    if (image.empty()) {
        fail("Cannot upload an empty scene atlas");
    }

    ensure_scene_atlas_texture(image.width, image.height);
    scene_atlas_texture_.width = image.width;
    scene_atlas_texture_.height = image.height;
    scene_atlas_texture_.format = VK_FORMAT_R8G8B8A8_UNORM;

    BufferAllocation staging_buffer = create_buffer(
        image.rgba_pixels.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void * mapped_data = nullptr;
    check_vk(vkMapMemory(device(), staging_buffer.memory, 0, image.rgba_pixels.size(), 0, &mapped_data), "Failed to map scene atlas staging buffer");
    std::memcpy(mapped_data, image.rgba_pixels.data(), image.rgba_pixels.size());
    vkUnmapMemory(device(), staging_buffer.memory);

    transition_image_layout(scene_atlas_texture_.handle, scene_atlas_texture_.format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer.handle, scene_atlas_texture_.handle, image.width, image.height);
    transition_image_layout(scene_atlas_texture_.handle, scene_atlas_texture_.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    destroy_buffer(staging_buffer);
}

void GpuResources::create_upload_command_pool() {
    const QueueFamilyIndices indices = context_->find_queue_families(physical_device(), context_->surface());

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();

    check_vk(vkCreateCommandPool(device(), &pool_info, nullptr, &upload_command_pool_), "Failed to create GPU upload command pool");
}

void GpuResources::create_static_quad_buffers() {
    constexpr std::array<float, 16> kQuadVertices = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    constexpr std::array<std::uint16_t, 6> kQuadIndices = {0, 1, 2, 2, 3, 0};

    static_quad_vertex_buffer_ = create_buffer(
        sizeof(kQuadVertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    static_quad_index_buffer_ = create_buffer(
        sizeof(kQuadIndices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void * vertex_data = nullptr;
    check_vk(vkMapMemory(device(), static_quad_vertex_buffer_.memory, 0, sizeof(kQuadVertices), 0, &vertex_data), "Failed to map quad vertex buffer");
    std::memcpy(vertex_data, kQuadVertices.data(), sizeof(kQuadVertices));
    vkUnmapMemory(device(), static_quad_vertex_buffer_.memory);

    void * index_data = nullptr;
    check_vk(vkMapMemory(device(), static_quad_index_buffer_.memory, 0, sizeof(kQuadIndices), 0, &index_data), "Failed to map quad index buffer");
    std::memcpy(index_data, kQuadIndices.data(), sizeof(kQuadIndices));
    vkUnmapMemory(device(), static_quad_index_buffer_.memory);
}

void GpuResources::ensure_instance_buffer_capacity(VkDeviceSize required_size) {
    if (instance_buffer_.handle != VK_NULL_HANDLE && instance_buffer_.size_bytes >= required_size) {
        return;
    }

    destroy_buffer(instance_buffer_);
    instance_buffer_ = create_buffer(
        required_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

void GpuResources::ensure_fog_texture(std::uint32_t width, std::uint32_t height) {
    const bool needs_new_texture =
        fog_texture_.handle == VK_NULL_HANDLE ||
        fog_texture_.width != width ||
        fog_texture_.height != height;
    if (!needs_new_texture) {
        return;
    }

    destroy_texture(fog_texture_);
    fog_texture_ = create_texture(
        width,
        height,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    check_vk(vkCreateSampler(device(), &sampler_info, nullptr, &fog_texture_.sampler), "Failed to create fog sampler");
    transition_image_layout(fog_texture_.handle, fog_texture_.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GpuResources::ensure_scene_atlas_texture(std::uint32_t width, std::uint32_t height) {
    const bool needs_new_texture =
        scene_atlas_texture_.handle == VK_NULL_HANDLE ||
        scene_atlas_texture_.width != width ||
        scene_atlas_texture_.height != height;
    if (!needs_new_texture) {
        return;
    }

    destroy_texture(scene_atlas_texture_);
    scene_atlas_texture_ = create_texture(
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    check_vk(vkCreateSampler(device(), &sampler_info, nullptr, &scene_atlas_texture_.sampler), "Failed to create scene atlas sampler");
    transition_image_layout(scene_atlas_texture_.handle, scene_atlas_texture_.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GpuResources::destroy_buffer(BufferAllocation & allocation) {
    if (allocation.handle != VK_NULL_HANDLE) {
        vkDestroyBuffer(device(), allocation.handle, nullptr);
    }
    if (allocation.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device(), allocation.memory, nullptr);
    }
    allocation = {};
}

void GpuResources::destroy_texture(TextureAllocation & allocation) {
    if (allocation.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device(), allocation.sampler, nullptr);
    }
    if (allocation.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device(), allocation.view, nullptr);
    }
    if (allocation.handle != VK_NULL_HANDLE) {
        vkDestroyImage(device(), allocation.handle, nullptr);
    }
    if (allocation.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device(), allocation.memory, nullptr);
    }
    allocation = {};
}

uint32_t GpuResources::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device(), &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool matches_type = (type_filter & (1u << i)) != 0;
        const bool matches_properties = (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;
        if (matches_type && matches_properties) {
            return i;
        }
    }

    fail("Failed to find a suitable GPU resource memory type");
}

BufferAllocation GpuResources::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) const {
    BufferAllocation allocation{};
    allocation.size_bytes = size;
    allocation.usage = usage;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateBuffer(device(), &buffer_info, nullptr, &allocation.handle), "Failed to create GPU buffer");

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device(), allocation.handle, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device(), &alloc_info, nullptr, &allocation.memory), "Failed to allocate GPU buffer memory");
    check_vk(vkBindBufferMemory(device(), allocation.handle, allocation.memory, 0), "Failed to bind GPU buffer memory");
    return allocation;
}

TextureAllocation GpuResources::create_texture(
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties
) const {
    TextureAllocation allocation{};
    allocation.width = width;
    allocation.height = height;
    allocation.format = format;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateImage(device(), &image_info, nullptr, &allocation.handle), "Failed to create GPU texture");

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device(), allocation.handle, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device(), &alloc_info, nullptr, &allocation.memory), "Failed to allocate GPU texture memory");
    check_vk(vkBindImageMemory(device(), allocation.handle, allocation.memory, 0), "Failed to bind GPU texture memory");

    allocation.view = create_image_view(allocation.handle, format);
    return allocation;
}

VkImageView GpuResources::create_image_view(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    check_vk(vkCreateImageView(device(), &view_info, nullptr, &image_view), "Failed to create GPU texture view");
    return image_view;
}

VkCommandBuffer GpuResources::begin_single_time_commands() const {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = upload_command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    check_vk(vkAllocateCommandBuffers(device(), &alloc_info, &command_buffer), "Failed to allocate GPU upload command buffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin GPU upload command buffer");
    return command_buffer;
}

void GpuResources::end_single_time_commands(VkCommandBuffer command_buffer) const {
    check_vk(vkEndCommandBuffer(command_buffer), "Failed to end GPU upload command buffer");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    check_vk(vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE), "Failed to submit GPU upload command buffer");
    check_vk(vkQueueWaitIdle(graphics_queue()), "Failed to wait for GPU upload queue");
    vkFreeCommandBuffers(device(), upload_command_pool_, 1, &command_buffer);
}

void GpuResources::transition_image_layout(
    VkImage image,
    VkFormat,
    VkImageLayout old_layout,
    VkImageLayout new_layout
) const {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        fail("Unsupported GPU resource image layout transition");
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    end_single_time_commands(command_buffer);
}

void GpuResources::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferCopy copy_region{};
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(command_buffer);
}

void GpuResources::copy_buffer_to_image(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height) const {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    end_single_time_commands(command_buffer);
}

VkDevice GpuResources::device() const {
    return context_ != nullptr ? context_->device() : VK_NULL_HANDLE;
}

VkPhysicalDevice GpuResources::physical_device() const {
    return context_ != nullptr ? context_->physical_device() : VK_NULL_HANDLE;
}

VkQueue GpuResources::graphics_queue() const {
    return context_ != nullptr ? context_->graphics_queue() : VK_NULL_HANDLE;
}

} // namespace gpu
