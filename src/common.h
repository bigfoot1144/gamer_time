#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <string>

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 720;
constexpr int kMaxFramesInFlight = 2;

[[noreturn]] inline void fail(const std::string& msg) {
    throw std::runtime_error(msg);
}

inline void check_vk(VkResult result, const char* msg) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(msg);
    }
}
