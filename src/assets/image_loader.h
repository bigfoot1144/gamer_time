#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct LoadedImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba_pixels;

    bool empty() const {
        return width == 0 || height == 0 || rgba_pixels.empty();
    }
};

namespace assets {

LoadedImage load_png_rgba(const std::string & image_path);

} // namespace assets
