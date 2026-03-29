#include "render/scene_renderer.h"

void SceneRenderer::initialize(SDL_Window * window, const std::string & shader_dir) {
    context_.shutdown();
    swapchain_.reset();
    resources_.reset();
    resources_.initialize_placeholders();
    text_overlay_ = TextOverlayRenderer{};
    context_.initialize(window);
    legacy_renderer_.init(window, shader_dir, context_, swapchain_);
}

void SceneRenderer::shutdown() {
    legacy_renderer_.shutdown();
    resources_.reset();
    swapchain_.shutdown(context_.device());
    context_.shutdown();
}

void SceneRenderer::request_resize() {
    legacy_renderer_.request_resize();
}

void SceneRenderer::set_overlay_text(std::string text) {
    text_overlay_.set_text(std::move(text));
    if (text_overlay_.take_dirty()) {
        legacy_renderer_.set_overlay_text(text_overlay_.text());
    }
}

void SceneRenderer::upload_frame_resources(
    const RenderBatch & batch,
    std::span<const std::uint8_t> fog_mask,
    std::uint32_t fog_width,
    std::uint32_t fog_height
) {
    resources_.upload_instance_data(batch.instances);
    resources_.upload_fog_mask(fog_mask, fog_width, fog_height);
}

void SceneRenderer::draw_frame() {
    uint32_t image_index = 0;
    if (!legacy_renderer_.begin_frame(image_index)) {
        return;
    }

    legacy_renderer_.record_frame(image_index);
    legacy_renderer_.submit_frame(image_index);
}

void SceneRenderer::wait_idle() {
    legacy_renderer_.wait_idle();
}
