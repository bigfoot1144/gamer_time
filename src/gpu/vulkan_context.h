#pragma once

#include "common.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <optional>
#include <string>
#include <vector>

namespace gpu {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    std::optional<uint32_t> compute_family;

    bool graphics_and_present_ready() const {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class VulkanContext {
public:
    void initialize(SDL_Window * window);
    void shutdown();
    void reset();

    QueueFamilyIndices find_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface) const;
    SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface) const;

    VkInstance instance() const { return instance_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    VkQueue present_queue() const { return present_queue_; }
    VkQueue compute_queue() const { return compute_queue_; }

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;

    void create_instance();
    void create_surface(SDL_Window * window);
    void pick_physical_device();
    void create_logical_device();
    bool check_device_extension_support(VkPhysicalDevice physical_device) const;
    bool is_device_suitable(VkPhysicalDevice physical_device) const;
};

} // namespace gpu
