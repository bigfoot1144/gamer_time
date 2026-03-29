#pragma once

#include "common.h"

#include <vector>

namespace gpu {

class SwapchainManager {
public:
    void reset();

    void bind_legacy_state(
        VkSwapchainKHR swapchain,
        VkFormat image_format,
        VkExtent2D extent,
        std::vector<VkImage> images,
        std::vector<VkImageView> image_views,
        std::vector<VkFramebuffer> framebuffers
    );

    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkFormat image_format() const { return image_format_; }
    VkExtent2D extent() const { return extent_; }
    const std::vector<VkImage> & images() const { return images_; }
    const std::vector<VkImageView> & image_views() const { return image_views_; }
    const std::vector<VkFramebuffer> & framebuffers() const { return framebuffers_; }

private:
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat image_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
    std::vector<VkFramebuffer> framebuffers_;
};

} // namespace gpu
