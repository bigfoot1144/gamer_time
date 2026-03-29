#pragma once

#include "common.h"

#include <cstdint>

namespace gpu {

struct TextureAllocation {
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

TextureAllocation make_texture_placeholder(std::uint32_t width, std::uint32_t height, VkFormat format);

} // namespace gpu
