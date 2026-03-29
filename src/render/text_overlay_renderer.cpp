#include "render/text_overlay_renderer.h"

#include "../external/SDL/src/render/SDL_render_debug_font.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>

namespace {

constexpr uint32_t kTextAtlasGlyphsPerRow = 14;
constexpr uint32_t kDebugFontCharacterSize = 8;
constexpr uint32_t kDebugFontGlyphPadding = 2;
constexpr size_t kMaxOverlayGlyphs = 4096;

} // namespace

void TextOverlayRenderer::initialize(
    gpu::VulkanContext & context,
    gpu::SwapchainManager & swapchain,
    gpu::GpuResources & resources,
    const std::string & shader_dir,
    VkRenderPass render_pass
) {
    shutdown();
    context_ = &context;
    swapchain_ = &swapchain;
    resources_ = &resources;
    shader_dir_ = shader_dir;
    render_pass_ = render_pass;
    dirty_ = true;
    create_descriptor_set_layout();
    create_font_resources();
    create_pipeline();
}

void TextOverlayRenderer::shutdown() {
    if (context_ != nullptr && device() != VK_NULL_HANDLE) {
        destroy_pipeline_objects();
        destroy_resource_objects();
    }

    context_ = nullptr;
    swapchain_ = nullptr;
    resources_ = nullptr;
    shader_dir_.clear();
    text_.clear();
    dirty_ = true;
    render_pass_ = VK_NULL_HANDLE;
    text_vertex_count_ = 0;
    text_vertex_capacity_ = 0;
}

void TextOverlayRenderer::on_render_pass_changed(VkRenderPass render_pass) {
    render_pass_ = render_pass;
    if (context_ == nullptr || device() == VK_NULL_HANDLE) {
        return;
    }

    destroy_pipeline_objects();
    create_pipeline();
    dirty_ = true;
}

void TextOverlayRenderer::set_text(std::string text) {
    if (text_ == text) {
        return;
    }

    text_ = std::move(text);
    dirty_ = true;
}

void TextOverlayRenderer::prepare_frame() {
    if (!dirty_ || text_vertex_buffer_ == VK_NULL_HANDLE || swapchain_ == nullptr) {
        return;
    }

    const VkExtent2D extent = swapchain_->extent();
    if (extent.width == 0 || extent.height == 0) {
        text_vertex_count_ = 0;
        dirty_ = false;
        return;
    }

    constexpr float margin_x = 24.0f;
    constexpr float margin_y = 24.0f;
    constexpr float glyph_scale = 2.0f;
    constexpr float glyph_width = kDebugFontCharacterSize * glyph_scale;
    constexpr float glyph_height = kDebugFontCharacterSize * glyph_scale;
    constexpr float line_gap = 6.0f;

    const float available_width = std::max(1.0f, static_cast<float>(extent.width) - margin_x * 2.0f);
    const float available_height = std::max(1.0f, static_cast<float>(extent.height) - margin_y * 2.0f);
    const uint32_t max_columns = std::max(1u, static_cast<uint32_t>(available_width / glyph_width));
    const uint32_t max_lines = std::max(1u, static_cast<uint32_t>(available_height / (glyph_height + line_gap)));

    std::vector<TextVertex> vertices;
    vertices.reserve(std::min(text_.size(), kMaxOverlayGlyphs) * 6);

    auto push_glyph = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
        vertices.push_back({{x0, y0}, {u0, v0}});
        vertices.push_back({{x1, y0}, {u1, v0}});
        vertices.push_back({{x1, y1}, {u1, v1}});
        vertices.push_back({{x0, y0}, {u0, v0}});
        vertices.push_back({{x1, y1}, {u1, v1}});
        vertices.push_back({{x0, y1}, {u0, v1}});
    };

    auto to_ndc_x = [&](float pixel_x) {
        return (pixel_x / static_cast<float>(extent.width)) * 2.0f - 1.0f;
    };
    auto to_ndc_y = [&](float pixel_y) {
        return 1.0f - (pixel_y / static_cast<float>(extent.height)) * 2.0f;
    };

    uint32_t column = 0;
    uint32_t line = 0;
    float pen_x = margin_x;
    float pen_y = margin_y;
    bool truncated = false;

    auto advance_line = [&]() {
        column = 0;
        ++line;
        pen_x = margin_x;
        pen_y = margin_y + line * (glyph_height + line_gap);
    };

    for (unsigned char raw_char : text_) {
        if (line >= max_lines || vertices.size() >= text_vertex_capacity_) {
            truncated = true;
            break;
        }

        if (raw_char == '\r') {
            continue;
        }

        if (raw_char == '\n') {
            advance_line();
            continue;
        }

        const int repeat = raw_char == '\t' ? 4 : 1;
        unsigned char glyph_char = raw_char == '\t' ? static_cast<unsigned char>(' ') : raw_char;
        if ((glyph_char < 32 && glyph_char != ' ') || glyph_char == 127) {
            glyph_char = '?';
        }
        if (glyph_char >= 128 && glyph_char <= 160) {
            glyph_char = '?';
        }

        for (int i = 0; i < repeat; ++i) {
            if (column >= max_columns) {
                advance_line();
            }
            if (line >= max_lines || vertices.size() >= text_vertex_capacity_) {
                truncated = true;
                break;
            }

            bool is_blank = false;
            const uint32_t glyph_index = glyph_index_for_byte(glyph_char, is_blank);
            if (!is_blank) {
                const uint32_t atlas_column = glyph_index % kTextAtlasGlyphsPerRow;
                const uint32_t atlas_row = glyph_index / kTextAtlasGlyphsPerRow;
                const float atlas_width = static_cast<float>((kDebugFontCharacterSize + kDebugFontGlyphPadding) * kTextAtlasGlyphsPerRow);
                const float atlas_height = static_cast<float>(((SDL_DEBUG_FONT_NUM_GLYPHS / kTextAtlasGlyphsPerRow) + 1) * (kDebugFontCharacterSize + kDebugFontGlyphPadding));
                const float atlas_x = static_cast<float>(atlas_column * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1);
                const float atlas_y = static_cast<float>(atlas_row * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1);

                const float x0 = to_ndc_x(pen_x);
                const float y0 = to_ndc_y(pen_y);
                const float x1 = to_ndc_x(pen_x + glyph_width);
                const float y1 = to_ndc_y(pen_y + glyph_height);

                const float u0 = atlas_x / atlas_width;
                const float v0 = (atlas_y + kDebugFontCharacterSize) / atlas_height;
                const float u1 = (atlas_x + kDebugFontCharacterSize) / atlas_width;
                const float v1 = atlas_y / atlas_height;

                push_glyph(x0, y0, x1, y1, u0, v0, u1, v1);
            }

            pen_x += glyph_width;
            ++column;
        }
    }

    if (truncated && max_lines > 0) {
        std::cerr << "Text overlay truncated to fit the temporary debug font buffer\n";
    }

    text_vertex_count_ = static_cast<uint32_t>(vertices.size());
    if (text_vertex_count_ == 0) {
        dirty_ = false;
        return;
    }

    void * mapped_data = nullptr;
    const VkDeviceSize upload_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(TextVertex));
    check_vk(vkMapMemory(device(), text_vertex_buffer_memory_, 0, upload_size, 0, &mapped_data), "Failed to map text vertex buffer");
    std::memcpy(mapped_data, vertices.data(), static_cast<size_t>(upload_size));
    vkUnmapMemory(device(), text_vertex_buffer_memory_);

    dirty_ = false;
}

void TextOverlayRenderer::record(VkCommandBuffer command_buffer) const {
    if (text_vertex_count_ == 0 || pipeline_ == VK_NULL_HANDLE || text_vertex_buffer_ == VK_NULL_HANDLE) {
        return;
    }

    const VkBuffer vertex_buffers[] = {text_vertex_buffer_};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout_,
        0,
        1,
        &descriptor_set_,
        0,
        nullptr
    );
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
    vkCmdDraw(command_buffer, text_vertex_count_, 1, 0, 0);
}

void TextOverlayRenderer::create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding sampler_binding{};
    sampler_binding.binding = 0;
    sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &sampler_binding;

    check_vk(vkCreateDescriptorSetLayout(device(), &layout_info, nullptr, &descriptor_set_layout_), "Failed to create text descriptor set layout");
}

void TextOverlayRenderer::create_font_resources() {
    constexpr uint32_t atlas_width = (kDebugFontCharacterSize + kDebugFontGlyphPadding) * kTextAtlasGlyphsPerRow;
    constexpr uint32_t atlas_rows = (SDL_DEBUG_FONT_NUM_GLYPHS / kTextAtlasGlyphsPerRow) + 1;
    constexpr uint32_t atlas_height = atlas_rows * (kDebugFontCharacterSize + kDebugFontGlyphPadding);

    std::vector<uint8_t> atlas_pixels(static_cast<size_t>(atlas_width) * atlas_height * 4, 0);

    for (uint32_t glyph = 0; glyph < SDL_DEBUG_FONT_NUM_GLYPHS; ++glyph) {
        const uint32_t atlas_column = glyph % kTextAtlasGlyphsPerRow;
        const uint32_t atlas_row = glyph / kTextAtlasGlyphsPerRow;
        const uint32_t atlas_x = atlas_column * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1;
        const uint32_t atlas_y = atlas_row * (kDebugFontCharacterSize + kDebugFontGlyphPadding) + 1;
        const Uint8 * char_data = SDL_RenderDebugTextFontData + (glyph * kDebugFontCharacterSize);

        for (uint32_t y = 0; y < kDebugFontCharacterSize; ++y) {
            for (uint32_t x = 0; x < kDebugFontCharacterSize; ++x) {
                const bool pixel_on = (char_data[y] & (1u << x)) != 0;
                const size_t dst_index = ((static_cast<size_t>(atlas_y + y) * atlas_width) + atlas_x + x) * 4;

                atlas_pixels[dst_index + 0] = 255;
                atlas_pixels[dst_index + 1] = 255;
                atlas_pixels[dst_index + 2] = 255;
                atlas_pixels[dst_index + 3] = pixel_on ? 255 : 0;
            }
        }
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    create_buffer(
        atlas_pixels.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer,
        staging_buffer_memory
    );

    void * mapped_data = nullptr;
    check_vk(vkMapMemory(device(), staging_buffer_memory, 0, atlas_pixels.size(), 0, &mapped_data), "Failed to map atlas staging memory");
    std::memcpy(mapped_data, atlas_pixels.data(), atlas_pixels.size());
    vkUnmapMemory(device(), staging_buffer_memory);

    create_image(
        atlas_width,
        atlas_height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        font_atlas_image_,
        font_atlas_image_memory_
    );

    transition_image_layout(font_atlas_image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer, font_atlas_image_, atlas_width, atlas_height);
    transition_image_layout(font_atlas_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device(), staging_buffer, nullptr);
    vkFreeMemory(device(), staging_buffer_memory, nullptr);

    font_atlas_image_view_ = create_image_view(font_atlas_image_, VK_FORMAT_R8G8B8A8_UNORM);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    check_vk(vkCreateSampler(device(), &sampler_info, nullptr, &font_atlas_sampler_), "Failed to create font atlas sampler");

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    check_vk(vkCreateDescriptorPool(device(), &pool_info, nullptr, &descriptor_pool_), "Failed to create text descriptor pool");

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    check_vk(vkAllocateDescriptorSets(device(), &alloc_info, &descriptor_set_), "Failed to allocate text descriptor set");

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = font_atlas_image_view_;
    image_info.sampler = font_atlas_sampler_;

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);

    text_vertex_capacity_ = kMaxOverlayGlyphs * 6;
    create_buffer(
        text_vertex_capacity_ * sizeof(TextVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        text_vertex_buffer_,
        text_vertex_buffer_memory_
    );

    if (resources_ != nullptr) {
        resources_->bind_text_overlay_resources(font_atlas_image_, font_atlas_image_view_, font_atlas_sampler_, text_vertex_buffer_);
    }
}

void TextOverlayRenderer::create_pipeline() {
    if (render_pass_ == VK_NULL_HANDLE) {
        return;
    }

    const auto vert_code = load_spirv_file(shader_dir_ + "/text.vert.spv");
    const auto frag_code = load_spirv_file(shader_dir_ + "/text.frag.spv");

    const VkShaderModule vert_shader_module = create_shader_module(vert_code);
    const VkShaderModule frag_shader_module = create_shader_module(frag_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(TextVertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(TextVertex, position);
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(TextVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_->extent().width);
    viewport.height = static_cast<float>(swapchain_->extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain_->extent();

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;

    check_vk(vkCreatePipelineLayout(device(), &pipeline_layout_info, nullptr, &pipeline_layout_), "Failed to create text pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    check_vk(vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_), "Failed to create text pipeline");

    vkDestroyShaderModule(device(), frag_shader_module, nullptr);
    vkDestroyShaderModule(device(), vert_shader_module, nullptr);
}

void TextOverlayRenderer::destroy_pipeline_objects() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

void TextOverlayRenderer::destroy_resource_objects() {
    if (text_vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device(), text_vertex_buffer_, nullptr);
        text_vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (text_vertex_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device(), text_vertex_buffer_memory_, nullptr);
        text_vertex_buffer_memory_ = VK_NULL_HANDLE;
    }
    if (font_atlas_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device(), font_atlas_sampler_, nullptr);
        font_atlas_sampler_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device(), font_atlas_image_view_, nullptr);
        font_atlas_image_view_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device(), font_atlas_image_, nullptr);
        font_atlas_image_ = VK_NULL_HANDLE;
    }
    if (font_atlas_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device(), font_atlas_image_memory_, nullptr);
        font_atlas_image_memory_ = VK_NULL_HANDLE;
    }
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device(), descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
        descriptor_set_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device(), descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }
}

uint32_t TextOverlayRenderer::glyph_index_for_byte(unsigned char c, bool & is_blank) const {
    is_blank = false;

    if (c <= 32) {
        is_blank = true;
        return 0;
    }

    if (c >= 127 && c <= 160) {
        c = '?';
    }

    if (c < 127) {
        return static_cast<uint32_t>(c - 33);
    }

    return static_cast<uint32_t>(c - 67);
}

uint32_t TextOverlayRenderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device(), &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool matches_type = (type_filter & (1u << i)) != 0;
        const bool matches_properties = (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;
        if (matches_type && matches_properties) {
            return i;
        }
    }

    fail("Failed to find a suitable memory type for text resources");
}

void TextOverlayRenderer::create_buffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer & buffer,
    VkDeviceMemory & buffer_memory
) const {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateBuffer(device(), &buffer_info, nullptr, &buffer), "Failed to create text buffer");

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(device(), buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device(), &alloc_info, nullptr, &buffer_memory), "Failed to allocate text buffer memory");
    check_vk(vkBindBufferMemory(device(), buffer, buffer_memory, 0), "Failed to bind text buffer memory");
}

void TextOverlayRenderer::create_image(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage & image,
    VkDeviceMemory & image_memory
) const {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check_vk(vkCreateImage(device(), &image_info, nullptr, &image), "Failed to create text image");

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(device(), image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    check_vk(vkAllocateMemory(device(), &alloc_info, nullptr, &image_memory), "Failed to allocate text image memory");
    check_vk(vkBindImageMemory(device(), image, image_memory, 0), "Failed to bind text image memory");
}

VkImageView TextOverlayRenderer::create_image_view(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    check_vk(vkCreateImageView(device(), &view_info, nullptr, &image_view), "Failed to create text image view");
    return image_view;
}

void TextOverlayRenderer::transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) const {
    const gpu::QueueFamilyIndices indices = context_->find_queue_families(physical_device(), context_->surface());
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();
    check_vk(vkCreateCommandPool(device(), &pool_info, nullptr, &command_pool), "Failed to create text transition command pool");

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    check_vk(vkAllocateCommandBuffers(device(), &alloc_info, &command_buffer), "Failed to allocate text transition command buffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin text transition command buffer");

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        fail("Unsupported text image layout transition");
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    check_vk(vkEndCommandBuffer(command_buffer), "Failed to end text transition command buffer");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    check_vk(vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE), "Failed to submit text transition command buffer");
    check_vk(vkQueueWaitIdle(graphics_queue()), "Failed to wait for text transition queue");
    vkFreeCommandBuffers(device(), command_pool, 1, &command_buffer);
    vkDestroyCommandPool(device(), command_pool, nullptr);
}

void TextOverlayRenderer::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
    const gpu::QueueFamilyIndices indices = context_->find_queue_families(physical_device(), context_->surface());
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();
    check_vk(vkCreateCommandPool(device(), &pool_info, nullptr, &command_pool), "Failed to create text copy command pool");

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    check_vk(vkAllocateCommandBuffers(device(), &alloc_info, &command_buffer), "Failed to allocate text copy command buffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin text copy command buffer");

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    check_vk(vkEndCommandBuffer(command_buffer), "Failed to end text copy command buffer");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    check_vk(vkQueueSubmit(graphics_queue(), 1, &submit_info, VK_NULL_HANDLE), "Failed to submit text copy command buffer");
    check_vk(vkQueueWaitIdle(graphics_queue()), "Failed to wait for text copy queue");
    vkFreeCommandBuffers(device(), command_pool, 1, &command_buffer);
    vkDestroyCommandPool(device(), command_pool, nullptr);
}

std::vector<uint32_t> TextOverlayRenderer::load_spirv_file(const std::string & path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        fail("Failed to open shader file: " + path);
    }

    const size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size % sizeof(uint32_t) != 0) {
        fail("Shader file size is not aligned to 4 bytes: " + path);
    }

    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(file_size));
    return buffer;
}

VkShaderModule TextOverlayRenderer::create_shader_module(const std::vector<uint32_t> & code) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    VkShaderModule shader_module = VK_NULL_HANDLE;
    check_vk(vkCreateShaderModule(device(), &create_info, nullptr, &shader_module), "Failed to create text shader module");
    return shader_module;
}

VkDevice TextOverlayRenderer::device() const {
    return context_ != nullptr ? context_->device() : VK_NULL_HANDLE;
}

VkPhysicalDevice TextOverlayRenderer::physical_device() const {
    return context_ != nullptr ? context_->physical_device() : VK_NULL_HANDLE;
}

VkQueue TextOverlayRenderer::graphics_queue() const {
    return context_ != nullptr ? context_->graphics_queue() : VK_NULL_HANDLE;
}
