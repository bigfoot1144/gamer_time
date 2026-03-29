#include "gpu/vulkan_context.h"

#include <set>

namespace {

constexpr const char * kAppName = "gamer_time";

} // namespace

namespace gpu {

void VulkanContext::initialize(SDL_Window * window) {
    shutdown();
    create_instance();
    create_surface(window);
    pick_physical_device();
    create_logical_device();
}

void VulkanContext::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }

    reset();
}

void VulkanContext::reset() {
    instance_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    physical_device_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    graphics_queue_ = VK_NULL_HANDLE;
    present_queue_ = VK_NULL_HANDLE;
    compute_queue_ = VK_NULL_HANDLE;
}

void VulkanContext::create_instance() {
    uint32_t extension_count = 0;
    const char * const * sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!sdl_extensions) {
        fail(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    std::vector<const char *> extensions(sdl_extensions, sdl_extensions + extension_count);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = kAppName;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = kAppName;
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    check_vk(vkCreateInstance(&create_info, nullptr, &instance_), "Failed to create Vulkan instance");
}

void VulkanContext::create_surface(SDL_Window * window) {
    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {
        fail(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
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

bool VulkanContext::check_device_extension_support(VkPhysicalDevice physical_device) const {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const auto & extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

bool VulkanContext::is_device_suitable(VkPhysicalDevice physical_device) const {
    const QueueFamilyIndices indices = find_queue_families(physical_device, surface_);
    const bool extensions_supported = check_device_extension_support(physical_device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        const SwapchainSupportDetails swapchain_support = query_swapchain_support(physical_device, surface_);
        swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
    }

    return indices.graphics_and_present_ready() && extensions_supported && swapchain_adequate;
}

void VulkanContext::pick_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        fail("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (is_device_suitable(device)) {
            physical_device_ = device;
            break;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        fail("Failed to find a suitable Vulkan GPU");
    }
}

void VulkanContext::create_logical_device() {
    const QueueFamilyIndices indices = find_queue_families(physical_device_, surface_);
    std::set<uint32_t> unique_queue_families = {
        indices.graphics_family.value(),
        indices.present_family.value(),
    };
    if (indices.compute_family.has_value()) {
        unique_queue_families.insert(indices.compute_family.value());
    }

    const float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_queue_families.size());
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    const std::vector<const char *> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    check_vk(vkCreateDevice(physical_device_, &create_info, nullptr, &device_), "Failed to create logical device");

    vkGetDeviceQueue(device_, indices.graphics_family.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, indices.present_family.value(), 0, &present_queue_);
    if (indices.compute_family.has_value()) {
        vkGetDeviceQueue(device_, indices.compute_family.value(), 0, &compute_queue_);
    }
}

} // namespace gpu
