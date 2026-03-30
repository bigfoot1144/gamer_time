#include "assets/image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/llama.cpp/vendor/stb/stb_image.h"

#include "common.h"

namespace assets {

LoadedImage load_png_rgba(const std::string & image_path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc * pixels = stbi_load(image_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        fail("Failed to load atlas image: " + image_path);
    }

    LoadedImage image{};
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.rgba_pixels.assign(pixels, pixels + (static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u));
    stbi_image_free(pixels);
    return image;
}

} // namespace assets
