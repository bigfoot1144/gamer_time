#pragma once

#include "common.h"

#include <cstddef>

namespace gpu {

struct BufferAllocation {
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size_bytes = 0;
    VkBufferUsageFlags usage = 0;
};

BufferAllocation make_buffer_placeholder(VkDeviceSize size_bytes, VkBufferUsageFlags usage);

} // namespace gpu
