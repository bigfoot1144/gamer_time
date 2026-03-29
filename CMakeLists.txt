cmake_minimum_required(VERSION 3.24...4.0)

project(SDL3_Vulkan_Llama LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_LLAMACPP_VULKAN "Enable Vulkan backend for llama.cpp" ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/SDL)

find_package(Vulkan REQUIRED)

if(ENABLE_LLAMACPP_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
else()
    set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
endif()

set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
add_subdirectory(external/llama.cpp)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

function(gt_configure_target target_name)
    target_include_directories(${target_name} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(gt_core INTERFACE)
target_include_directories(gt_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(gt_ai STATIC
    src/ai/llama_controller.cpp
    src/ai/token_stream.cpp
    src/llama_worker.cpp
)
gt_configure_target(gt_ai)
target_link_libraries(gt_ai PUBLIC
    gt_core
    llama
)
if(UNIX AND NOT APPLE)
    target_link_libraries(gt_ai PUBLIC Threads::Threads dl)
endif()

add_library(gt_platform STATIC
    src/platform/camera_controller.cpp
    src/platform/sdl_platform.cpp
)
gt_configure_target(gt_platform)
target_link_libraries(gt_platform PUBLIC
    gt_core
    SDL3::SDL3-static
)

add_library(gt_game STATIC
    src/game/command_queue.cpp
    src/game/fog_of_war_system.cpp
    src/game/navigation_system.cpp
    src/game/selection_system.cpp
    src/game/world.cpp
)
gt_configure_target(gt_game)
target_link_libraries(gt_game PUBLIC
    gt_core
    gt_platform
)

add_library(gt_gpu STATIC
    src/gpu/buffer_utils.cpp
    src/gpu/gpu_resources.cpp
    src/gpu/swapchain_manager.cpp
    src/gpu/texture_utils.cpp
    src/gpu/vulkan_context.cpp
    src/render/text_overlay_renderer.cpp
)
gt_configure_target(gt_gpu)
target_link_libraries(gt_gpu PUBLIC
    gt_core
    Vulkan::Vulkan
    SDL3::SDL3-static
)

add_library(gt_render STATIC
    src/render/batch_builder.cpp
    src/render/depth_sorter.cpp
    src/render/frustum_culler.cpp
    src/render/projection_system.cpp
    src/render/render_extractor.cpp
    src/render/scene_renderer.cpp
)
gt_configure_target(gt_render)
target_link_libraries(gt_render PUBLIC
    gt_core
    gt_game
    gt_gpu
    gt_platform
)

add_library(gt_app STATIC
    src/app/application.cpp
)
gt_configure_target(gt_app)
target_link_libraries(gt_app PUBLIC
    gt_ai
    gt_game
    gt_platform
    gt_render
)

add_executable(game
    src/main.cpp
)
gt_configure_target(game)
target_link_libraries(game PRIVATE
    gt_app
)

set(DEFAULT_MODEL_PATH "../model/Qwen3-VL-4B-Instruct-Q4_K_M.gguf")
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

find_program(GLSLANG_VALIDATOR glslangValidator)

set(SHADER_SOURCES
    "${SHADER_DIR}/triangle.vert"
    "${SHADER_DIR}/triangle.frag"
    "${SHADER_DIR}/text.vert"
    "${SHADER_DIR}/text.frag"
)

set(SPIRV_OUTPUTS "")
if(GLSLANG_VALIDATOR)
    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV "${SHADER}.spv")

        add_custom_command(
            OUTPUT "${SPIRV}"
            COMMAND "${GLSLANG_VALIDATOR}" -V "${SHADER}" -o "${SPIRV}"
            DEPENDS "${SHADER}"
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS "${SPIRV}")
    endforeach()

    add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(game shaders)
else()
    message(WARNING "glslangValidator not found. Shader .spv files will not be built automatically.")
endif()

add_custom_target(run
    COMMAND $<TARGET_FILE:game> "${DEFAULT_MODEL_PATH}" "${SHADER_DIR}"
    DEPENDS game
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Running game with default model: ${DEFAULT_MODEL_PATH}"
)
