#include "gpu/swapchain_manager.h"

#include <algorithm>
#include <limits>

namespace gpu {

void SwapchainManager::initialize(const VulkanContext & context, SDL_Window * window, VkRenderPass render_pass) {
    shutdown(context.device());
    create_swapchain(context, window);
    create_image_views(context.device());
    if (render_pass != VK_NULL_HANDLE) {
        create_framebuffers(context.device(), render_pass);
    }
}

void SwapchainManager::shutdown(VkDevice device) {
    if (device != VK_NULL_HANDLE) {
        for (VkFramebuffer framebuffer : framebuffers_) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        for (VkImageView image_view : image_views_) {
            vkDestroyImageView(device, image_view, nullptr);
        }
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain_, nullptr);
        }
    }

    reset();
}

void SwapchainManager::recreate(const VulkanContext & context, SDL_Window * window, VkRenderPass render_pass) {
    int width = 0;
    int height = 0;
    do {
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width == 0 || height == 0) {
            SDL_WaitEvent(nullptr);
        }
    } while (width == 0 || height == 0);

    check_vk(vkDeviceWaitIdle(context.device()), "Failed to wait for device idle during swapchain recreation");
    shutdown(context.device());
    create_swapchain(context, window);
    create_image_views(context.device());
    if (render_pass != VK_NULL_HANDLE) {
        create_framebuffers(context.device(), render_pass);
    }
}

void SwapchainManager::rebuild_framebuffers(VkDevice device, VkRenderPass render_pass) {
    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers_.clear();

    if (render_pass != VK_NULL_HANDLE) {
        create_framebuffers(device, render_pass);
    }
}

void SwapchainManager::reset() {
    swapchain_ = VK_NULL_HANDLE;
    image_format_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
    images_.clear();
    image_views_.clear();
    framebuffers_.clear();
}

VkSurfaceFormatKHR SwapchainManager::choose_surface_format(const std::vector<VkSurfaceFormatKHR> & available_formats) const {
    for (const auto & available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR SwapchainManager::choose_present_mode(const std::vector<VkPresentModeKHR> & available_present_modes) const {
    for (const auto & available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainManager::choose_extent(SDL_Window * window, const VkSurfaceCapabilitiesKHR & capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);

    VkExtent2D actual_extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
    };

    actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actual_extent;
}

void SwapchainManager::create_swapchain(const VulkanContext & context, SDL_Window * window) {
    const SwapchainSupportDetails swapchain_support =
        context.query_swapchain_support(context.physical_device(), context.surface());
    const VkSurfaceFormatKHR surface_format = choose_surface_format(swapchain_support.formats);
    const VkPresentModeKHR present_mode = choose_present_mode(swapchain_support.present_modes);
    const VkExtent2D extent = choose_extent(window, swapchain_support.capabilities);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    const QueueFamilyIndices indices =
        context.find_queue_families(context.physical_device(), context.surface());
    const uint32_t queue_family_indices[] = {
        indices.graphics_family.value(),
        indices.present_family.value(),
    };

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = context.surface();
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    check_vk(vkCreateSwapchainKHR(context.device(), &create_info, nullptr, &swapchain_), "Failed to create swapchain");

    vkGetSwapchainImagesKHR(context.device(), swapchain_, &image_count, nullptr);
    images_.resize(image_count);
    vkGetSwapchainImagesKHR(context.device(), swapchain_, &image_count, images_.data());

    image_format_ = surface_format.format;
    extent_ = extent;
}

void SwapchainManager::create_image_views(VkDevice device) {
    image_views_.resize(images_.size());

    for (size_t i = 0; i < images_.size(); ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = image_format_;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        check_vk(vkCreateImageView(device, &view_info, nullptr, &image_views_[i]), "Failed to create image view");
    }
}

void SwapchainManager::create_framebuffers(VkDevice device, VkRenderPass render_pass) {
    framebuffers_.resize(image_views_.size());

    for (size_t i = 0; i < image_views_.size(); ++i) {
        VkImageView attachments[] = {image_views_[i]};

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = extent_.width;
        framebuffer_info.height = extent_.height;
        framebuffer_info.layers = 1;

        check_vk(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers_[i]), "Failed to create framebuffer");
    }
}

} // namespace gpu
