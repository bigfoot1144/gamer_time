#include "gpu/vulkan_context.h"

namespace gpu {

void VulkanContext::reset() {
    instance_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    physical_device_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    graphics_queue_ = VK_NULL_HANDLE;
    present_queue_ = VK_NULL_HANDLE;
    compute_queue_ = VK_NULL_HANDLE;
}

QueueFamilyIndices VulkanContext::find_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface) const {
    QueueFamilyIndices indices;

    if (physical_device == VK_NULL_HANDLE) {
        return indices;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const VkQueueFamilyProperties & family = queue_families[i];
        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && !indices.graphics_family.has_value()) {
            indices.graphics_family = i;
        }
        if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            const bool is_dedicated_compute = (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0;
            if (!indices.compute_family.has_value() || is_dedicated_compute) {
                indices.compute_family = i;
            }
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
            if (present_support && !indices.present_family.has_value()) {
                indices.present_family = i;
            }
        }
    }

    if (!indices.compute_family.has_value()) {
        indices.compute_family = indices.graphics_family;
    }

    return indices;
}

SwapchainSupportDetails VulkanContext::query_swapchain_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface) const {
    SwapchainSupportDetails details{};
    if (physical_device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
        return details;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    if (format_count > 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data());
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    if (present_mode_count > 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

void VulkanContext::bind_legacy_handles(
    VkInstance instance,
    VkSurfaceKHR surface,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkQueue compute_queue
) {
    instance_ = instance;
    surface_ = surface;
    physical_device_ = physical_device;
    device_ = device;
    graphics_queue_ = graphics_queue;
    present_queue_ = present_queue;
    compute_queue_ = compute_queue;
}

} // namespace gpu
