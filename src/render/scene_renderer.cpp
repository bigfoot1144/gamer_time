#include "render/scene_renderer.h"

#include <array>
#include <fstream>

namespace {

struct ScenePushConstants {
    float camera_center[2];
    float viewport_size[2];
    float fog_size[2];
    float zoom;
};

} // namespace

void SceneRenderer::initialize(SDL_Window * window, const std::string & shader_dir) {
    shutdown();
    window_ = window;
    shader_dir_ = shader_dir;
    framebuffer_resized_ = false;
    current_frame_ = 0;

    context_.initialize(window_);
    resources_.initialize(context_);
    swapchain_.initialize(context_, window_, VK_NULL_HANDLE);
    create_render_pass();
    create_scene_descriptor_set_layout();
    create_scene_descriptor_resources();
    create_graphics_pipeline();
    swapchain_.rebuild_framebuffers(context_.device(), render_pass_);
    create_command_pool();
    create_command_buffers();
    create_sync_objects();
    text_overlay_.initialize(context_, swapchain_, resources_, shader_dir_, render_pass_);
    initialized_ = true;
}

void SceneRenderer::shutdown() {
    if (!initialized_ && context_.device() == VK_NULL_HANDLE) {
        window_ = nullptr;
        shader_dir_.clear();
        framebuffer_resized_ = false;
        current_frame_ = 0;
        return;
    }

    if (context_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device());
    }

    cleanup_swapchain_dependent_state();

    for (size_t i = 0; i < image_available_semaphores_.size(); ++i) {
        vkDestroySemaphore(context_.device(), image_available_semaphores_[i], nullptr);
        vkDestroySemaphore(context_.device(), render_finished_semaphores_[i], nullptr);
        vkDestroyFence(context_.device(), in_flight_fences_[i], nullptr);
    }
    image_available_semaphores_.clear();
    render_finished_semaphores_.clear();
    in_flight_fences_.clear();

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device(), command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
    if (scene_descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device(), scene_descriptor_pool_, nullptr);
        scene_descriptor_pool_ = VK_NULL_HANDLE;
        scene_descriptor_set_ = VK_NULL_HANDLE;
    }
    if (scene_descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device(), scene_descriptor_set_layout_, nullptr);
        scene_descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    resources_.shutdown();
    swapchain_.shutdown(context_.device());
    context_.shutdown();

    window_ = nullptr;
    shader_dir_.clear();
    framebuffer_resized_ = false;
    current_frame_ = 0;
    initialized_ = false;
}

void SceneRenderer::request_resize() {
    framebuffer_resized_ = true;
}

void SceneRenderer::set_overlay_text(std::string text) {
    text_overlay_.set_text(std::move(text));
}

void SceneRenderer::upload_frame_resources(
    const RenderBatch & batch,
    std::span<const std::uint8_t> fog_mask,
    std::uint32_t fog_width,
    std::uint32_t fog_height,
    const CameraState & camera
) {
    resources_.upload_instance_data(batch.instances);
    resources_.upload_fog_mask(fog_mask, fog_width, fog_height);
    camera_ = camera;
    update_scene_descriptor_set();
}

void SceneRenderer::draw_frame() {
    if (!initialized_ || context_.device() == VK_NULL_HANDLE || swapchain_.swapchain() == VK_NULL_HANDLE) {
        return;
    }

    update_scene_descriptor_set();
    text_overlay_.prepare_frame();

    uint32_t image_index = 0;
    check_vk(vkWaitForFences(context_.device(), 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX), "Failed to wait for in-flight fence");

    const VkResult acquire_result = vkAcquireNextImageKHR(
        context_.device(),
        swapchain_.swapchain(),
        UINT64_MAX,
        image_available_semaphores_[current_frame_],
        VK_NULL_HANDLE,
        &image_index
    );

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        fail("Failed to acquire swapchain image");
    }

    check_vk(vkResetFences(context_.device(), 1, &in_flight_fences_[current_frame_]), "Failed to reset in-flight fence");
    check_vk(vkResetCommandBuffer(command_buffers_[image_index], 0), "Failed to reset command buffer");

    record_command_buffer(command_buffers_[image_index], image_index);

    VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame_]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    check_vk(vkQueueSubmit(context_.graphics_queue(), 1, &submit_info, in_flight_fences_[current_frame_]), "Failed to submit draw command buffer");

    VkSwapchainKHR swapchains[] = {swapchain_.swapchain()};
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    const VkResult present_result = vkQueuePresentKHR(context_.present_queue(), &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    } else if (present_result != VK_SUCCESS) {
        fail("Failed to present swapchain image");
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

void SceneRenderer::wait_idle() {
    if (context_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device());
    }
}

void SceneRenderer::create_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_.image_format();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    check_vk(vkCreateRenderPass(context_.device(), &render_pass_info, nullptr, &render_pass_), "Failed to create render pass");
}

void SceneRenderer::create_scene_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding fog_binding{};
    fog_binding.binding = 0;
    fog_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fog_binding.descriptorCount = 1;
    fog_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &fog_binding;

    check_vk(vkCreateDescriptorSetLayout(context_.device(), &layout_info, nullptr, &scene_descriptor_set_layout_), "Failed to create scene descriptor set layout");
}

void SceneRenderer::create_scene_descriptor_resources() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    check_vk(vkCreateDescriptorPool(context_.device(), &pool_info, nullptr, &scene_descriptor_pool_), "Failed to create scene descriptor pool");

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = scene_descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &scene_descriptor_set_layout_;

    check_vk(vkAllocateDescriptorSets(context_.device(), &alloc_info, &scene_descriptor_set_), "Failed to allocate scene descriptor set");
    update_scene_descriptor_set();
}

void SceneRenderer::update_scene_descriptor_set() {
    if (scene_descriptor_set_ == VK_NULL_HANDLE) {
        return;
    }

    const gpu::TextureAllocation & fog = resources_.fog_texture();
    if (fog.view == VK_NULL_HANDLE || fog.sampler == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = fog.view;
    image_info.sampler = fog.sampler;

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = scene_descriptor_set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(context_.device(), 1, &descriptor_write, 0, nullptr);
}

std::vector<uint32_t> SceneRenderer::load_spirv_file(const std::string & path) const {
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

VkShaderModule SceneRenderer::create_shader_module(const std::vector<uint32_t> & code) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    VkShaderModule shader_module = VK_NULL_HANDLE;
    check_vk(vkCreateShaderModule(context_.device(), &create_info, nullptr, &shader_module), "Failed to create shader module");
    return shader_module;
}

void SceneRenderer::create_graphics_pipeline() {
    const auto vert_code = load_spirv_file(shader_dir_ + "/triangle.vert.spv");
    const auto frag_code = load_spirv_file(shader_dir_ + "/triangle.frag.spv");

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

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription binding_descriptions[2]{};
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(float) * 4;
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_descriptions[1].binding = 1;
    binding_descriptions[1].stride = sizeof(InstanceData);
    binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 6> attribute_descriptions{};
    attribute_descriptions[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    attribute_descriptions[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2};
    attribute_descriptions[2] = {2, 1, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(InstanceData, world_pos))};
    attribute_descriptions[3] = {3, 1, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(InstanceData, size))};
    attribute_descriptions[4] = {4, 1, VK_FORMAT_R32_UINT, static_cast<uint32_t>(offsetof(InstanceData, sprite_index))};
    attribute_descriptions[5] = {5, 1, VK_FORMAT_R32_UINT, static_cast<uint32_t>(offsetof(InstanceData, flags))};

    vertex_input_info.vertexBindingDescriptionCount = 2;
    vertex_input_info.pVertexBindingDescriptions = binding_descriptions;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_.extent().width);
    viewport.height = static_cast<float>(swapchain_.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain_.extent();

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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &scene_descriptor_set_layout_;
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(ScenePushConstants);
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    check_vk(vkCreatePipelineLayout(context_.device(), &pipeline_layout_info, nullptr, &pipeline_layout_), "Failed to create pipeline layout");

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

    check_vk(vkCreateGraphicsPipelines(context_.device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_), "Failed to create graphics pipeline");

    vkDestroyShaderModule(context_.device(), frag_shader_module, nullptr);
    vkDestroyShaderModule(context_.device(), vert_shader_module, nullptr);
}

void SceneRenderer::create_command_pool() {
    const gpu::QueueFamilyIndices indices = context_.find_queue_families(context_.physical_device(), context_.surface());

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();

    check_vk(vkCreateCommandPool(context_.device(), &pool_info, nullptr, &command_pool_), "Failed to create command pool");
}

void SceneRenderer::create_command_buffers() {
    command_buffers_.resize(swapchain_.framebuffers().size());

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

    check_vk(vkAllocateCommandBuffers(context_.device(), &alloc_info, command_buffers_.data()), "Failed to allocate command buffers");
}

void SceneRenderer::create_sync_objects() {
    image_available_semaphores_.resize(kMaxFramesInFlight);
    render_finished_semaphores_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        check_vk(vkCreateSemaphore(context_.device(), &semaphore_info, nullptr, &image_available_semaphores_[i]), "Failed to create image_available semaphore");
        check_vk(vkCreateSemaphore(context_.device(), &semaphore_info, nullptr, &render_finished_semaphores_[i]), "Failed to create render_finished semaphore");
        check_vk(vkCreateFence(context_.device(), &fence_info, nullptr, &in_flight_fences_[i]), "Failed to create in_flight fence");
    }
}

void SceneRenderer::cleanup_swapchain_dependent_state() {
    text_overlay_.shutdown();

    if (graphics_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device(), graphics_pipeline_, nullptr);
        graphics_pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (swapchain_.swapchain() != VK_NULL_HANDLE) {
        swapchain_.rebuild_framebuffers(context_.device(), VK_NULL_HANDLE);
    }

    if (!command_buffers_.empty() && command_pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(context_.device(), command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }

    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device(), render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
}

void SceneRenderer::recreate_swapchain() {
    vkDeviceWaitIdle(context_.device());
    cleanup_swapchain_dependent_state();
    swapchain_.recreate(context_, window_, VK_NULL_HANDLE);
    create_render_pass();
    create_graphics_pipeline();
    swapchain_.rebuild_framebuffers(context_.device(), render_pass_);
    create_command_buffers();
    update_scene_descriptor_set();
    text_overlay_.initialize(context_, swapchain_, resources_, shader_dir_, render_pass_);
}

void SceneRenderer::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    check_vk(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin command buffer");

    VkClearValue clear_color = {{{0.08f, 0.09f, 0.12f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = swapchain_.framebuffers()[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_.extent();
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
    const VkBuffer vertex_buffers[] = {
        resources_.static_quad_vertex_buffer().handle,
        resources_.instance_buffer().handle,
    };
    const VkDeviceSize offsets[] = {0, 0};
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout_,
        0,
        1,
        &scene_descriptor_set_,
        0,
        nullptr
    );
    vkCmdBindVertexBuffers(command_buffer, 0, 2, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buffer, resources_.static_quad_index_buffer().handle, 0, VK_INDEX_TYPE_UINT16);

    const gpu::TextureAllocation & fog = resources_.fog_texture();
    ScenePushConstants push_constants{};
    push_constants.camera_center[0] = camera_.world_center.x;
    push_constants.camera_center[1] = camera_.world_center.y;
    push_constants.viewport_size[0] = static_cast<float>(swapchain_.extent().width);
    push_constants.viewport_size[1] = static_cast<float>(swapchain_.extent().height);
    push_constants.fog_size[0] = static_cast<float>(fog.width);
    push_constants.fog_size[1] = static_cast<float>(fog.height);
    push_constants.zoom = camera_.zoom;
    vkCmdPushConstants(
        command_buffer,
        pipeline_layout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(ScenePushConstants),
        &push_constants
    );

    const uint32_t instance_count = static_cast<uint32_t>(resources_.staged_instances().size());
    if (instance_count > 0) {
        vkCmdDrawIndexed(command_buffer, 6, instance_count, 0, 0, 0);
    }

    text_overlay_.record(command_buffer);

    vkCmdEndRenderPass(command_buffer);

    check_vk(vkEndCommandBuffer(command_buffer), "Failed to record command buffer");
}
