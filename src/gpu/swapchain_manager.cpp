#include "gpu/swapchain_manager.h"

#include <utility>

namespace gpu {

void SwapchainManager::reset() {
    swapchain_ = VK_NULL_HANDLE;
    image_format_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
    images_.clear();
    image_views_.clear();
    framebuffers_.clear();
}

void SwapchainManager::bind_legacy_state(
    VkSwapchainKHR swapchain,
    VkFormat image_format,
    VkExtent2D extent,
    std::vector<VkImage> images,
    std::vector<VkImageView> image_views,
    std::vector<VkFramebuffer> framebuffers
) {
    swapchain_ = swapchain;
    image_format_ = image_format;
    extent_ = extent;
    images_ = std::move(images);
    image_views_ = std::move(image_views);
    framebuffers_ = std::move(framebuffers);
}

} // namespace gpu
