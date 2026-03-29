#include "render/text_overlay_renderer.h"

#include <utility>

void TextOverlayRenderer::set_text(std::string text) {
    if (text_ == text) {
        return;
    }

    text_ = std::move(text);
    dirty_ = true;
}

bool TextOverlayRenderer::take_dirty() {
    const bool was_dirty = dirty_;
    dirty_ = false;
    return was_dirty;
}
