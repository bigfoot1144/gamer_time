#include "gpu/buffer_utils.h"

namespace gpu {

BufferAllocation make_buffer_placeholder(VkDeviceSize size_bytes, VkBufferUsageFlags usage) {
    BufferAllocation allocation{};
    allocation.size_bytes = size_bytes;
    allocation.usage = usage;
    return allocation;
}

} // namespace gpu
