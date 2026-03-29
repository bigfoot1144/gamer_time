#pragma once

#include "common.h"
#include "gpu/gpu_resources.h"
#include "gpu/swapchain_manager.h"
#include "gpu/vulkan_context.h"
#include "platform/camera_controller.h"
#include "render/render_world.h"
#include "render/text_overlay_renderer.h"

#include <span>
#include <string>
#include <vector>

struct SDL_Window;

class SceneRenderer {
public:
    void initialize(SDL_Window * window, const std::string & shader_dir);
    void shutdown();

    void request_resize();
    void set_overlay_text(std::string text);
    void upload_frame_resources(
        const RenderBatch & batch,
        std::span<const std::uint8_t> fog_mask,
        std::uint32_t fog_width,
        std::uint32_t fog_height,
        const CameraState & camera
    );
    void draw_frame();
    void wait_idle();

    const gpu::GpuResources & resources() const { return resources_; }

private:
    SDL_Window * window_ = nullptr;
    std::string shader_dir_;
    bool framebuffer_resized_ = false;
    CameraState camera_{};

    gpu::VulkanContext context_;
    gpu::SwapchainManager swapchain_;
    gpu::GpuResources resources_;
    TextOverlayRenderer text_overlay_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout scene_descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool scene_descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet scene_descriptor_set_ = VK_NULL_HANDLE;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    size_t current_frame_ = 0;
    bool initialized_ = false;

    void create_render_pass();
    void create_scene_descriptor_set_layout();
    void create_scene_descriptor_resources();
    void update_scene_descriptor_set();
    void create_graphics_pipeline();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();
    void cleanup_swapchain_dependent_state();
    void recreate_swapchain();
    std::vector<uint32_t> load_spirv_file(const std::string & path) const;
    VkShaderModule create_shader_module(const std::vector<uint32_t> & code) const;
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);
};
