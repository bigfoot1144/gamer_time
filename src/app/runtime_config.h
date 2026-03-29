#pragma once

#include "common.h"

#include <string>

struct RuntimeConfig {
    std::string model_path;
    std::string shader_dir = "shaders";
    int initial_width = kInitialWidth;
    int initial_height = kInitialHeight;
};
