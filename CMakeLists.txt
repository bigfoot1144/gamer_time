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

# Automatically pick up all .cpp files under src/
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(GAME_SOURCES STREQUAL "")
    message(FATAL_ERROR "No source files found in ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_executable(game ${GAME_SOURCES})

target_include_directories(game PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(game PRIVATE
    SDL3::SDL3-static
    Vulkan::Vulkan
    llama
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(game PRIVATE Threads::Threads dl)
endif()

if(MSVC)
    target_compile_options(game PRIVATE /W4)
else()
    target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(DEFAULT_MODEL_PATH "/home/bigfoot/dev/game_idea/model/Qwen3-VL-4B-Instruct-Q4_K_M.gguf")
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

# Optional shader compilation if glslangValidator is installed
find_program(GLSLANG_VALIDATOR glslangValidator)

set(SHADER_SOURCES
    "${SHADER_DIR}/triangle.vert"
    "${SHADER_DIR}/triangle.frag"
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