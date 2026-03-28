#include "vulkan_renderer.h"

#include "../external/SDL/src/render/SDL_render_debug_font.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace {

constexpr uint32_t kTextAtlasGlyphsPerRow = 14;
constexpr uint32_t kDebugFontCharacterSize = 8;
constexpr uint32_t kDebugFontGlyphPadding = 2;
constexpr size_t kMaxOverlayGlyphs = 4096;

uint32_t glyph_index_for_byte(unsigned char c, bool & is_blank) {
    is_blank = false;

    if (c <= 32) {
        is_blank = true;
        return 0;
    }

    if (c >= 127 && c <= 160) {
        c = '?';
    }

    if (c < 127) {
        return static_cast<uint32_t>(c - 33);
    }

    return static_cast<uint32_t>(c - 67);
}

} // namespace

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

void VulkanRenderer::init(SDL_Window* window, const std::string& shader_dir) {
    shutdown();
    window_ = window;
    shader_dir_ = shader_dir;
    overlay_text_.clear();
    text_dirty_ = true;

    create_instance();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_text_descriptor_set_layout();
    create_graphics_pipeline();
    create_text_pipeline();
    create_framebuffers();
    create_command_pool();
    create_text_resources();
    create_command_buffers();
    create_sync_objects();
    initialized_ = true;
}

void VulkanRenderer::shutdown() {
    if (!window_ && !initialized_) {
        return;
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    cleanup_swapchain();

    for (size_t i = 0; i < image_available_semaphores_.size(); ++i) {
        vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
        vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
        vkDestroyFence(device_, in_flight_fences_[i], nullptr);
    }
    image_available_semaphores_.clear();
    render_finished_semaphores_.clear();
    in_flight_fences_.clear();

    if (text_vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, text_vertex_buffer_, nullptr);
        text_vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (text_vertex_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, text_vertex_buffer_memory_, nullptr);
        text_vertex_buffer_memory_ = VK_NULL_HANDLE;
    }
    text_vertex_capacity_ = 0;
    text_vertex_count_ = 0;

    if (font_atlas_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, font_atlas_sampler_, nullptr);
        font_atlas_sampler_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, font_atlas_image_view_, nullptr);
        font_atlas_image_view_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, font_atlas_image_, nullptr);
        font_atlas_image_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, font_atlas_image_memory_, nullptr);
        font_atlas_image_memory_ = VK_NULL_HANDLE;
    }

    if (text_descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, text_descriptor_pool_, nullptr);
        text_descriptor_pool_ = VK_NULL_HANDLE;
        text_descriptor_set_ = VK_NULL_HANDLE;
    }
    if (text_descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, text_descriptor_set_layout_, nullptr);
        text_descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    swapchain_images_.clear();
    overlay_text_.clear();
    window_ = nullptr;
    initialized_ = false;
    framebuffer_resized_ = false;
    text_dirty_ = true;
    current_frame_ = 0;
}

void VulkanRenderer::on_window_resized() {
    framebuffer_resized_ = true;
}

void VulkanRenderer::set_overlay_text(std::string text) {
    if (overlay_text_ == text) {
        return;
    }

    overlay_text_ = std::move(text);
    text_dirty_ = true;
}

void VulkanRenderer::wait_idle() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

void VulkanRenderer::create_instance() {
    uint32_t ext_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&ext_count);
    if (!sdl_extensions) {
        fail(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + ext_count);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "SDL3 Vulkan Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "custom";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    check_vk(vkCreateInstance(&create_info, nullptr, &instance_), "Failed to create Vulkan instance");
}

void VulkanRenderer::create_surface() {
    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        fail(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
}

QueueFamilyIndices VulkanRenderer::find_queue_families(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present_support);
        if (present_support) {
            indices.present_family = i;
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool VulkanRenderer::check_device_extension_support(VkPhysicalDevice device) const {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

SwapchainSupportDetails VulkanRenderer::query_swapchain_support(VkPhysicalDevice device) const {
    SwapchainSupportDetails details{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
    if (format_count > 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, details.formats.data());
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr);
    if (present_mode_count > 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, details.present_modes.data());
    }

    return details;
}

bool VulkanRenderer::is_device_suitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = find_queue_families(device);
    const bool extensions_supported = check_device_extension_support(device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        const auto swapchain_support = query_swapchain_support(device);
        swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
    }

    return indices.complete() && extensions_supported && swapchain_adequate;
}

void VulkanRenderer::pick_physical_device() {
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

void VulkanRenderer::create_logical_device() {
    const QueueFamilyIndices indices = find_queue_families(physical_device_);
    std::set<uint32_t> unique_queue_families = {
        indices.graphics_family.value(),
        indices.present_family.value(),
    };

    const float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    const std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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
}

VkSurfaceFormatKHR VulkanRenderer::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const {
    for (const auto& available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }
    return available_formats[0];
}

VkPresentModeKHR VulkanRenderer::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);

    VkExtent2D actual_extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
    };

    actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actual_extent;
}

uint32_t VulkanRenderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool matches_type = (type_filter & (1u << i)) != 0;
        const bool matches_properties =
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;
        if (matches_type && matches_properties) {
            return i;
        }
    }

    fail("Failed to find a suitable memory type");
}

void VulkanRenderer::create_buffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& buffer_memory
) const {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device_, &alloc_info, nullptr, &buffer_memory), "Failed to allocate buffer memory");
    check_vk(vkBindBufferMemory(device_, buffer, buffer_memory, 0), "Failed to bind buffer memory");
}

void VulkanRenderer::create_image(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& image_memory
) const {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateImage(device_, &image_info, nullptr, &image), "Failed to create image");

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device_, image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device_, &alloc_info, nullptr, &image_memory), "Failed to allocate image memory");
    check_vk(vkBindImageMemory(device_, image, image_memory, 0), "Failed to bind image memory");
}

VkImageView VulkanRenderer::create_image_view(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    check_vk(vkCreateImageView(device_, &view_info, nullptr, &image_view), "Failed to create image view");
    return image_view;
}

VkCommandBuffer VulkanRenderer::begin_single_time_commands() const {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    check_vk(vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer), "Failed to allocate one-time command buffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin one-time command buffer");
    return command_buffer;
}

void VulkanRenderer::end_single_time_commands(VkCommandBuffer command_buffer) const {
    check_vk(vkEndCommandBuffer(command_buffer), "Failed to end one-time command buffer");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    check_vk(vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE), "Failed to submit one-time command buffer");
    check_vk(vkQueueWaitIdle(graphics_queue_), "Failed to wait for graphics queue");
    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

void VulkanRenderer::transition_image_layout(
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout
) const {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        fail("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    end_single_time_commands(command_buffer);
}

void VulkanRenderer::copy_buffer_to_image(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height
) const {
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    end_single_time_commands(command_buffer);
}

void VulkanRenderer::create_swapchain() {
    const auto swapchain_support = query_swapchain_support(physical_device_);
    const auto surface_format = choose_swap_surface_format(swapchain_support.formats);
    const auto present_mode = choose_swap_present_mode(swapchain_support.present_modes);
    const auto extent = choose_swap_extent(swapchain_support.capabilities);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    const QueueFamilyIndices indices = find_queue_families(physical_device_);
    const uint32_t queue_family_indices[] = {
        indices.graphics_family.value(),
        indices.present_family.value(),
    };

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
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

    check_vk(vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_), "Failed to create swapchain");

    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

    swapchain_image_format_ = surface_format.format;
    swapchain_extent_ = extent;
}

void VulkanRenderer::create_image_views() {
    swapchain_image_views_.resize(swapchain_images_.size());

    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        swapchain_image_views_[i] = create_image_view(swapchain_images_[i], swapchain_image_format_);
    }
}

void VulkanRenderer::create_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_image_format_;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    check_vk(vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_), "Failed to create render pass");
}

std::vector<uint32_t> VulkanRenderer::load_spirv_file(const std::string& path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        fail("Failed to open shader file: " + path);
    }

    const size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size % sizeof(uint32_t) != 0) {
        fail("Shader file size is not aligned to 4 bytes: " + path);
    }

    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
    return buffer;
}

VkShaderModule VulkanRenderer::create_shader_module(const std::vector<uint32_t>& code) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    VkShaderModule shader_module = VK_NULL_HANDLE;
    check_vk(vkCreateShaderModule(device_, &create_info, nullptr, &shader_module), "Failed to create shader module");
    return shader_module;
}

void VulkanRenderer::create_graphics_pipeline() {
    const auto vert_code = load_spirv_file(shader_dir_ + "/triangle.vert.spv");
    const auto frag_code = load_spirv_file(shader_dir_ + "/triangle.frag.spv");

    const VkShaderModule vert_shader_module = create_shader_module(vert_code);
    const VkShaderModule frag_shader_module = create_shader_module(frag_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    check_vk(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout_), "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    check_vk(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_), "Failed to create graphics pipeline");

    vkDestroyShaderModule(device_, frag_shader_module, nullptr);
    vkDestroyShaderModule(device_, vert_shader_module, nullptr);
}

void VulkanRenderer::create_text_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding sampler_binding{};
    sampler_binding.binding = 0;
    sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &sampler_binding;

    check_vk(
        vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &text_descriptor_set_layout_),
        "Failed to create text descriptor set layout"
    );
}

void VulkanRenderer::create_text_resources() {
    constexpr uint32_t atlas_width = (kDebugFontCharacterSize + kDebugFontGlyphPadding) * kTextAtlasGlyphsPerRow;
    constexpr uint32_t atlas_rows = (SDL_DEBUG_FONT_NUM_GLYPHS / kTextAtlasGlyphsPerRow) + 1;
    constexpr uint32_t atlas_height = atlas_rows * (kDebugFontCharacterSize + kDebugFontGlyphPadding);

    std::vector<uint8_t> atlas_pixels(static_cast<size_t>(atlas_width) * atlas_height * 4, 0);

    for (uint32_t glyph = 0; glyph < SDL_DEBUG_FONT_NUM_GLYPHS; ++glyph) {
        const uint32_t atlas_column = glyph % kTextAtlasGlyphsPerRow;
        const uint32_t atlas_row = glyph / kTextAtlasGlyphsPerRow;
        const uint32_t atlas_x = atlas_column * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1;
        const uint32_t atlas_y = atlas_row * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1;
        const Uint8* char_data = SDL_RenderDebugTextFontData + (glyph * kDebugFontCharacterSize);

        for (uint32_t y = 0; y < kDebugFontCharacterSize; ++y) {
            for (uint32_t x = 0; x < kDebugFontCharacterSize; ++x) {
                const bool pixel_on = (char_data[y] & (1u << x)) != 0;
                const size_t dst_index =
                    ((static_cast<size_t>(atlas_y + y) * atlas_width) + atlas_x + x) * 4;

                atlas_pixels[dst_index + 0] = 255;
                atlas_pixels[dst_index + 1] = 255;
                atlas_pixels[dst_index + 2] = 255;
                atlas_pixels[dst_index + 3] = pixel_on ? 255 : 0;
            }
        }
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    create_buffer(
        atlas_pixels.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer,
        staging_buffer_memory
    );

    void* mapped_data = nullptr;
    check_vk(
        vkMapMemory(device_, staging_buffer_memory, 0, atlas_pixels.size(), 0, &mapped_data),
        "Failed to map atlas staging memory"
    );
    std::memcpy(mapped_data, atlas_pixels.data(), atlas_pixels.size());
    vkUnmapMemory(device_, staging_buffer_memory);

    create_image(
        atlas_width,
        atlas_height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        font_atlas_image_,
        font_atlas_image_memory_
    );

    transition_image_layout(font_atlas_image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer, font_atlas_image_, atlas_width, atlas_height);
    transition_image_layout(font_atlas_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_buffer_memory, nullptr);

    font_atlas_image_view_ = create_image_view(font_atlas_image_, VK_FORMAT_R8G8B8A8_UNORM);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    check_vk(vkCreateSampler(device_, &sampler_info, nullptr, &font_atlas_sampler_), "Failed to create font atlas sampler");

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    check_vk(vkCreateDescriptorPool(device_, &pool_info, nullptr, &text_descriptor_pool_), "Failed to create text descriptor pool");

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = text_descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &text_descriptor_set_layout_;

    check_vk(vkAllocateDescriptorSets(device_, &alloc_info, &text_descriptor_set_), "Failed to allocate text descriptor set");

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = font_atlas_image_view_;
    image_info.sampler = font_atlas_sampler_;

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = text_descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);

    text_vertex_capacity_ = kMaxOverlayGlyphs * 6;
    create_buffer(
        text_vertex_capacity_ * sizeof(TextVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        text_vertex_buffer_,
        text_vertex_buffer_memory_
    );
}

void VulkanRenderer::create_text_pipeline() {
    const auto vert_code = load_spirv_file(shader_dir_ + "/text.vert.spv");
    const auto frag_code = load_spirv_file(shader_dir_ + "/text.frag.spv");

    const VkShaderModule vert_shader_module = create_shader_module(vert_code);
    const VkShaderModule frag_shader_module = create_shader_module(frag_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(TextVertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(TextVertex, position);
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(TextVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &text_descriptor_set_layout_;

    check_vk(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &text_pipeline_layout_), "Failed to create text pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = text_pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    check_vk(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &text_pipeline_), "Failed to create text pipeline");

    vkDestroyShaderModule(device_, frag_shader_module, nullptr);
    vkDestroyShaderModule(device_, vert_shader_module, nullptr);
}

void VulkanRenderer::create_framebuffers() {
    framebuffers_.resize(swapchain_image_views_.size());

    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[] = {swapchain_image_views_[i]};

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass_;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = swapchain_extent_.width;
        framebuffer_info.height = swapchain_extent_.height;
        framebuffer_info.layers = 1;

        check_vk(vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[i]), "Failed to create framebuffer");
    }
}

void VulkanRenderer::create_command_pool() {
    const QueueFamilyIndices indices = find_queue_families(physical_device_);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();

    check_vk(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_), "Failed to create command pool");
}

void VulkanRenderer::create_command_buffers() {
    command_buffers_.resize(framebuffers_.size());

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

    check_vk(vkAllocateCommandBuffers(device_, &alloc_info, command_buffers_.data()), "Failed to allocate command buffers");
}

void VulkanRenderer::create_sync_objects() {
    image_available_semaphores_.resize(kMaxFramesInFlight);
    render_finished_semaphores_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        check_vk(vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]), "Failed to create image_available semaphore");
        check_vk(vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]), "Failed to create render_finished semaphore");
        check_vk(vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]), "Failed to create in_flight fence");
    }
}

void VulkanRenderer::cleanup_swapchain() {
    if (text_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, text_pipeline_, nullptr);
        text_pipeline_ = VK_NULL_HANDLE;
    }

    if (text_pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, text_pipeline_layout_, nullptr);
        text_pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (graphics_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
        graphics_pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();

    if (!command_buffers_.empty() && command_pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }

    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    for (VkImageView image_view : swapchain_image_views_) {
        vkDestroyImageView(device_, image_view, nullptr);
    }
    swapchain_image_views_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::recreate_swapchain() {
    int width = 0;
    int height = 0;
    do {
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        if (width == 0 || height == 0) {
            SDL_WaitEvent(nullptr);
        }
    } while (width == 0 || height == 0);

    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_text_pipeline();
    create_framebuffers();
    create_command_buffers();
    text_dirty_ = true;
}

void VulkanRenderer::update_text_vertices() {
    if (text_vertex_buffer_ == VK_NULL_HANDLE || swapchain_extent_.width == 0 || swapchain_extent_.height == 0) {
        text_vertex_count_ = 0;
        text_dirty_ = false;
        return;
    }

    constexpr float margin_x = 24.0f;
    constexpr float margin_y = 24.0f;
    constexpr float glyph_scale = 2.0f;
    constexpr float glyph_width = kDebugFontCharacterSize * glyph_scale;
    constexpr float glyph_height = kDebugFontCharacterSize * glyph_scale;
    constexpr float line_gap = 6.0f;

    const float available_width = std::max(1.0f, static_cast<float>(swapchain_extent_.width) - margin_x * 2.0f);
    const float available_height = std::max(1.0f, static_cast<float>(swapchain_extent_.height) - margin_y * 2.0f);
    const uint32_t max_columns = std::max(1u, static_cast<uint32_t>(available_width / glyph_width));
    const uint32_t max_lines = std::max(1u, static_cast<uint32_t>(available_height / (glyph_height + line_gap)));

    std::vector<TextVertex> vertices;
    vertices.reserve(std::min(overlay_text_.size(), kMaxOverlayGlyphs) * 6);

    auto push_glyph = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
        vertices.push_back({{x0, y0}, {u0, v0}});
        vertices.push_back({{x1, y0}, {u1, v0}});
        vertices.push_back({{x1, y1}, {u1, v1}});
        vertices.push_back({{x0, y0}, {u0, v0}});
        vertices.push_back({{x1, y1}, {u1, v1}});
        vertices.push_back({{x0, y1}, {u0, v1}});
    };

    auto to_ndc_x = [&](float pixel_x) {
        return (pixel_x / static_cast<float>(swapchain_extent_.width)) * 2.0f - 1.0f;
    };
    auto to_ndc_y = [&](float pixel_y) {
        return 1.0f - (pixel_y / static_cast<float>(swapchain_extent_.height)) * 2.0f;
    };

    uint32_t column = 0;
    uint32_t line = 0;
    float pen_x = margin_x;
    float pen_y = margin_y;
    bool truncated = false;

    auto advance_line = [&]() {
        column = 0;
        ++line;
        pen_x = margin_x;
        pen_y = margin_y + line * (glyph_height + line_gap);
    };

    for (unsigned char raw_char : overlay_text_) {
        if (line >= max_lines || vertices.size() >= text_vertex_capacity_) {
            truncated = true;
            break;
        }

        if (raw_char == '\r') {
            continue;
        }

        if (raw_char == '\n') {
            advance_line();
            continue;
        }

        const int repeat = raw_char == '\t' ? 4 : 1;
        unsigned char glyph_char = raw_char == '\t' ? static_cast<unsigned char>(' ') : raw_char;
        if ((glyph_char < 32 && glyph_char != ' ') || glyph_char == 127) {
            glyph_char = '?';
        }
        if (glyph_char >= 128 && glyph_char <= 160) {
            glyph_char = '?';
        }

        for (int i = 0; i < repeat; ++i) {
            if (column >= max_columns) {
                advance_line();
            }
            if (line >= max_lines || vertices.size() >= text_vertex_capacity_) {
                truncated = true;
                break;
            }

            bool is_blank = false;
            const uint32_t glyph_index = glyph_index_for_byte(glyph_char, is_blank);
            if (!is_blank) {
                const uint32_t atlas_column = glyph_index % kTextAtlasGlyphsPerRow;
                const uint32_t atlas_row = glyph_index / kTextAtlasGlyphsPerRow;
                const float atlas_width = static_cast<float>((kDebugFontCharacterSize + kDebugFontGlyphPadding) * kTextAtlasGlyphsPerRow);
                const float atlas_height = static_cast<float>(((SDL_DEBUG_FONT_NUM_GLYPHS / kTextAtlasGlyphsPerRow) + 1) * (kDebugFontCharacterSize + kDebugFontGlyphPadding));
                const float atlas_x = static_cast<float>(atlas_column * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1);
                const float atlas_y = static_cast<float>(atlas_row * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1);

                const float x0 = to_ndc_x(pen_x);
                const float y0 = to_ndc_y(pen_y);
                const float x1 = to_ndc_x(pen_x + glyph_width);
                const float y1 = to_ndc_y(pen_y + glyph_height);

                const float u0 = atlas_x / atlas_width;
                const float v0 = (atlas_y + kDebugFontCharacterSize) / atlas_height;
                const float u1 = (atlas_x + kDebugFontCharacterSize) / atlas_width;
                const float v1 = atlas_y / atlas_height;

                push_glyph(x0, y0, x1, y1, u0, v0, u1, v1);
            }

            pen_x += glyph_width;
            ++column;
        }
    }

    if (truncated && max_lines > 0) {
        std::cerr << "Text overlay truncated to fit the temporary debug font buffer\n";
    }

    text_vertex_count_ = static_cast<uint32_t>(vertices.size());
    if (text_vertex_count_ == 0) {
        text_dirty_ = false;
        return;
    }

    void* mapped_data = nullptr;
    const VkDeviceSize upload_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(TextVertex));
    check_vk(vkMapMemory(device_, text_vertex_buffer_memory_, 0, upload_size, 0, &mapped_data), "Failed to map text vertex buffer");
    std::memcpy(mapped_data, vertices.data(), static_cast<size_t>(upload_size));
    vkUnmapMemory(device_, text_vertex_buffer_memory_);

    text_dirty_ = false;
}

void VulkanRenderer::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin command buffer");

    VkClearValue clear_color = {{{0.08f, 0.09f, 0.12f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = framebuffers_[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_extent_;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    if (text_vertex_count_ > 0) {
        const VkBuffer vertex_buffers[] = {text_vertex_buffer_};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, text_pipeline_);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            text_pipeline_layout_,
            0,
            1,
            &text_descriptor_set_,
            0,
            nullptr
        );
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdDraw(command_buffer, text_vertex_count_, 1, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    check_vk(vkEndCommandBuffer(command_buffer), "Failed to record command buffer");
}

void VulkanRenderer::draw_frame() {
    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return;
    }

    if (text_dirty_) {
        update_text_vertices();
    }

    check_vk(vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX), "Failed to wait for in-flight fence");

    uint32_t image_index = 0;
    const VkResult acquire_result = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        image_available_semaphores_[current_frame_],
        VK_NULL_HANDLE,
        &image_index
    );

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        fail("Failed to acquire swapchain image");
    }

    check_vk(vkResetFences(device_, 1, &in_flight_fences_[current_frame_]), "Failed to reset in-flight fence");
    check_vk(vkResetCommandBuffer(command_buffers_[image_index], 0), "Failed to reset command buffer");
    record_command_buffer(command_buffers_[image_index], image_index);

    VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame_]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    check_vk(vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]), "Failed to submit draw command buffer");

    VkSwapchainKHR swapchains[] = {swapchain_};
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    const VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    } else if (present_result != VK_SUCCESS) {
        fail("Failed to present swapchain image");
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}
