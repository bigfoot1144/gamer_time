#include "gpu/texture_utils.h"

namespace gpu {

TextureAllocation make_texture_placeholder(std::uint32_t width, std::uint32_t height, VkFormat format) {
    TextureAllocation allocation{};
    allocation.width = width;
    allocation.height = height;
    allocation.format = format;
    return allocation;
}

} // namespace gpu
