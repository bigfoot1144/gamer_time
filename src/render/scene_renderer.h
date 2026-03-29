#pragma once

#include "gpu/gpu_resources.h"
#include "gpu/swapchain_manager.h"
#include "gpu/vulkan_context.h"
#include "render/render_world.h"
#include "render/text_overlay_renderer.h"
#include "vulkan_renderer.h"

#include <span>
#include <string>

struct SDL_Window;

class SceneRenderer {
public:
    void initialize(SDL_Window * window, const std::string & shader_dir);
    void shutdown();

    void request_resize();
    void set_overlay_text(std::string text);
    void upload_frame_resources(const RenderBatch & batch, std::span<const std::uint8_t> fog_mask, std::uint32_t fog_width, std::uint32_t fog_height);
    void draw_frame();
    void wait_idle();

    const gpu::GpuResources & resources() const { return resources_; }

private:
    gpu::VulkanContext context_;
    gpu::SwapchainManager swapchain_;
    gpu::GpuResources resources_;
    TextOverlayRenderer text_overlay_;
    VulkanRenderer legacy_renderer_;
};
