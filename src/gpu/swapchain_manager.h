#pragma once

#include "common.h"
#include "gpu/vulkan_context.h"

#include <SDL3/SDL.h>

#include <vector>

namespace gpu {

class SwapchainManager {
public:
    void initialize(const VulkanContext & context, SDL_Window * window, VkRenderPass render_pass);
    void shutdown(VkDevice device);
    void recreate(const VulkanContext & context, SDL_Window * window, VkRenderPass render_pass);
    void rebuild_framebuffers(VkDevice device, VkRenderPass render_pass);
    void reset();

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

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR> & available_formats) const;
    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR> & available_present_modes) const;
    VkExtent2D choose_extent(SDL_Window * window, const VkSurfaceCapabilitiesKHR & capabilities) const;
    void create_swapchain(const VulkanContext & context, SDL_Window * window);
    void create_image_views(VkDevice device);
    void create_framebuffers(VkDevice device, VkRenderPass render_pass);
};

} // namespace gpu
