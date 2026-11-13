#include "rr/render/GpuRenderer.hpp"

#include "rr/render/Mat4.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#ifndef RUSTYRAGE_FOUNDATION_SHADER_DIR
#define RUSTYRAGE_FOUNDATION_SHADER_DIR "shaders/foundation"
#endif

namespace rr::render {
namespace {

struct Vertex {
    float position[3];
    float color[3];
};

std::array<Vertex, 36> build_cube() {
    constexpr float h = 0.7f;
    const std::array<std::array<float, 3>, 8> points{{
        {{-h, -h, -h}},
        {{h, -h, -h}},
        {{h, h, -h}},
        {{-h, h, -h}},
        {{-h, -h, h}},
        {{h, -h, h}},
        {{h, h, h}},
        {{-h, h, h}},
    }};

    const std::array<std::array<int, 4>, 6> faces{{
        {{4, 5, 6, 7}},
        {{1, 0, 3, 2}},
        {{0, 4, 7, 3}},
        {{5, 1, 2, 6}},
        {{3, 7, 6, 2}},
        {{0, 1, 5, 4}},
    }};

    const std::array<std::array<float, 3>, 6> colors{{
        {{0.95f, 0.35f, 0.25f}},
        {{0.25f, 0.65f, 0.95f}},
        {{0.35f, 0.85f, 0.45f}},
        {{0.95f, 0.80f, 0.25f}},
        {{0.75f, 0.40f, 0.95f}},
        {{0.90f, 0.90f, 0.90f}},
    }};

    std::array<Vertex, 36> vertices{};
    std::size_t cursor = 0;

    for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
        const auto& face = faces[face_index];
        const auto& color = colors[face_index];
        const std::array<int, 6> order{face[0], face[1], face[2],
                                       face[0], face[2], face[3]};

        for (int point_index : order) {
            const auto& point = points[static_cast<std::size_t>(point_index)];
            vertices[cursor++] = Vertex{{point[0], point[1], point[2]},
                                        {color[0], color[1], color[2]}};
        }
    }

    return vertices;
}

std::vector<Uint8> load_binary_file(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open shader: " + path;
        return {};
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        error = "empty shader file: " + path;
        return {};
    }

    std::vector<Uint8> data(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        error = "cannot read shader: " + path;
        return {};
    }
    return data;
}

std::string string_property(SDL_PropertiesID properties, const char* name) {
    const char* value = SDL_GetStringProperty(properties, name, nullptr);
    return value != nullptr ? value : "unknown";
}

} // namespace

struct GpuRenderer::Impl {
    SDL_GPUDevice* device{nullptr};
    SDL_Window* window{nullptr};
    SDL_GPUGraphicsPipeline* pipeline{nullptr};
    SDL_GPUBuffer* vertex_buffer{nullptr};
    SDL_GPUTransferBuffer* vertex_transfer{nullptr};
    SDL_GPUTexture* depth_texture{nullptr};
    SDL_GPUTextureFormat swapchain_format{SDL_GPU_TEXTUREFORMAT_INVALID};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t frames{0};
    GpuInfo gpu_info{};

    ~Impl() {
        // The owning GpuRenderer calls shutdown(); this is only a guard.
    }

    bool create_shader(bool vertex_stage,
                       const std::string& path,
                       Uint32 uniform_buffer_count,
                       SDL_GPUShader*& out_shader,
                       std::string& error) {
        const std::vector<Uint8> code = load_binary_file(path, error);
        if (code.empty()) {
            return false;
        }

        SDL_GPUShaderCreateInfo shader_info{};
        shader_info.code_size = code.size();
        shader_info.code = code.data();
        shader_info.entrypoint = "main";
        shader_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        shader_info.stage = vertex_stage ? SDL_GPU_SHADERSTAGE_VERTEX
                                         : SDL_GPU_SHADERSTAGE_FRAGMENT;
        shader_info.num_uniform_buffers = uniform_buffer_count;

        out_shader = SDL_CreateGPUShader(device, &shader_info);
        if (out_shader == nullptr) {
            error = "SDL_CreateGPUShader failed for " + path + ": " + SDL_GetError();
            return false;
        }
        return true;
    }

    bool create_pipeline(std::string& error) {
        const std::string shader_dir = RUSTYRAGE_FOUNDATION_SHADER_DIR;

        SDL_GPUShader* vertex_shader = nullptr;
        SDL_GPUShader* fragment_shader = nullptr;

        if (!create_shader(true, shader_dir + "/triangle.vert.spv", 1, vertex_shader, error) ||
            !create_shader(false, shader_dir + "/triangle.frag.spv", 0, fragment_shader, error)) {
            if (vertex_shader != nullptr) {
                SDL_ReleaseGPUShader(device, vertex_shader);
            }
            if (fragment_shader != nullptr) {
                SDL_ReleaseGPUShader(device, fragment_shader);
            }
            return false;
        }

        SDL_GPUVertexBufferDescription buffer_description{};
        buffer_description.slot = 0;
        buffer_description.pitch = sizeof(Vertex);
        buffer_description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        buffer_description.instance_step_rate = 0;

        SDL_GPUVertexAttribute attributes[2]{};
        attributes[0].location = 0;
        attributes[0].buffer_slot = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[0].offset = static_cast<Uint32>(offsetof(Vertex, position));
        attributes[1].location = 1;
        attributes[1].buffer_slot = 0;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = static_cast<Uint32>(offsetof(Vertex, color));

        SDL_GPUVertexInputState vertex_input{};
        vertex_input.vertex_buffer_descriptions = &buffer_description;
        vertex_input.num_vertex_buffers = 1;
        vertex_input.vertex_attributes = attributes;
        vertex_input.num_vertex_attributes = 2;

        SDL_GPURasterizerState rasterizer{};
        rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
        rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
        rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        rasterizer.enable_depth_clip = true;

        SDL_GPUMultisampleState multisample{};
        multisample.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUDepthStencilState depth_stencil{};
        depth_stencil.compare_op = SDL_GPU_COMPAREOP_LESS;
        depth_stencil.enable_depth_test = true;
        depth_stencil.enable_depth_write = true;

        SDL_GPUColorTargetDescription color_target{};
        color_target.format = swapchain_format;

        SDL_GPUGraphicsPipelineTargetInfo target_info{};
        target_info.color_target_descriptions = &color_target;
        target_info.num_color_targets = 1;
        target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        target_info.has_depth_stencil_target = true;

        SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.vertex_shader = vertex_shader;
        pipeline_info.fragment_shader = fragment_shader;
        pipeline_info.vertex_input_state = vertex_input;
        pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipeline_info.rasterizer_state = rasterizer;
        pipeline_info.multisample_state = multisample;
        pipeline_info.depth_stencil_state = depth_stencil;
        pipeline_info.target_info = target_info;

        pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);

        SDL_ReleaseGPUShader(device, vertex_shader);
        SDL_ReleaseGPUShader(device, fragment_shader);

        if (pipeline == nullptr) {
            error = std::string("SDL_CreateGPUGraphicsPipeline failed: ") + SDL_GetError();
            return false;
        }
        return true;
    }

    bool create_vertex_buffer(std::string& error) {
        const auto vertices = build_cube();
        const Uint32 byte_size = static_cast<Uint32>(sizeof(Vertex) * vertices.size());

        SDL_GPUBufferCreateInfo buffer_info{};
        buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        buffer_info.size = byte_size;
        vertex_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
        if (vertex_buffer == nullptr) {
            error = std::string("SDL_CreateGPUBuffer failed: ") + SDL_GetError();
            return false;
        }

        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = byte_size;
        vertex_transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
        if (vertex_transfer == nullptr) {
            error = std::string("SDL_CreateGPUTransferBuffer failed: ") + SDL_GetError();
            return false;
        }

        void* mapped = SDL_MapGPUTransferBuffer(device, vertex_transfer, false);
        if (mapped == nullptr) {
            error = std::string("SDL_MapGPUTransferBuffer failed: ") + SDL_GetError();
            return false;
        }
        std::memcpy(mapped, vertices.data(), byte_size);
        SDL_UnmapGPUTransferBuffer(device, vertex_transfer);

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            error = std::string("SDL_AcquireGPUCommandBuffer failed: ") + SDL_GetError();
            return false;
        }

        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command);
        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = vertex_transfer;
        source.offset = 0;
        SDL_GPUBufferRegion destination{};
        destination.buffer = vertex_buffer;
        destination.offset = 0;
        destination.size = byte_size;
        SDL_UploadToGPUBuffer(copy_pass, &source, &destination, true);
        SDL_EndGPUCopyPass(copy_pass);

        if (!SDL_SubmitGPUCommandBuffer(command)) {
            error = std::string("SDL_SubmitGPUCommandBuffer failed: ") + SDL_GetError();
            return false;
        }
        return true;
    }

    bool create_depth_texture(std::uint32_t new_width, std::uint32_t new_height) {
        if (depth_texture != nullptr) {
            SDL_ReleaseGPUTexture(device, depth_texture);
            depth_texture = nullptr;
        }
        if (new_width == 0 || new_height == 0) {
            width = new_width;
            height = new_height;
            return true;
        }

        SDL_GPUTextureCreateInfo texture_info{};
        texture_info.type = SDL_GPU_TEXTURETYPE_2D;
        texture_info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        texture_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        texture_info.width = new_width;
        texture_info.height = new_height;
        texture_info.layer_count_or_depth = 1;
        texture_info.num_levels = 1;
        texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

        depth_texture = SDL_CreateGPUTexture(device, &texture_info);
        if (depth_texture == nullptr) {
            return false;
        }

        width = new_width;
        height = new_height;
        return true;
    }

    void shutdown() {
        if (device != nullptr) {
            SDL_WaitForGPUIdle(device);

            if (pipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
                pipeline = nullptr;
            }
            if (vertex_buffer != nullptr) {
                SDL_ReleaseGPUBuffer(device, vertex_buffer);
                vertex_buffer = nullptr;
            }
            if (vertex_transfer != nullptr) {
                SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
                vertex_transfer = nullptr;
            }
            if (depth_texture != nullptr) {
                SDL_ReleaseGPUTexture(device, depth_texture);
                depth_texture = nullptr;
            }

            if (window != nullptr) {
                SDL_ReleaseWindowFromGPUDevice(device, window);
                window = nullptr;
            }
        }
    }
};

GpuRenderer::GpuRenderer() = default;

GpuRenderer::~GpuRenderer() {
    shutdown();
}

bool GpuRenderer::initialize(SDL_Window* window, std::string& error) {
    if (impl_ != nullptr) {
        error = "renderer already initialized";
        return false;
    }

    impl_ = std::make_unique<Impl>();
    Impl& impl = *impl_;

    impl.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (impl.device == nullptr) {
        error = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        impl_ = nullptr;
        return false;
    }

    const char* driver = SDL_GetGPUDeviceDriver(impl.device);
    impl.gpu_info.backend = driver != nullptr ? driver : "unknown";

    SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(impl.device);
    impl.gpu_info.device = string_property(properties, SDL_PROP_GPU_DEVICE_NAME_STRING);
    impl.gpu_info.driver = string_property(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING);
    impl.gpu_info.driver_version =
        string_property(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING);
    impl.gpu_info.driver_info =
        string_property(properties, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING);

    if (!SDL_ClaimWindowForGPUDevice(impl.device, window)) {
        error = std::string("SDL_ClaimWindowForGPUDevice failed: ") + SDL_GetError();
        shutdown();
        return false;
    }
    impl.window = window;

    impl.swapchain_format = SDL_GetGPUSwapchainTextureFormat(impl.device, window);
    if (impl.swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        error = std::string("SDL_GetGPUSwapchainTextureFormat failed: ") + SDL_GetError();
        shutdown();
        return false;
    }

    // Prefer IMMEDIATE so the bounded smoke test does not depend on
    // compositor vsync, but keep the backend default (VSYNC) when the
    // platform does not support it. The actual backend/present mode is
    // recorded in the report; no runtime pass is inferred from this call.
    if (SDL_WindowSupportsGPUPresentMode(impl.device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        if (!SDL_SetGPUSwapchainParameters(impl.device,
                                           window,
                                           SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                           SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            std::cerr << "[WARN] SDL_SetGPUSwapchainParameters(IMMEDIATE) failed: "
                      << SDL_GetError() << '\n';
        }
    }

    if (!impl.create_pipeline(error) ||
        !impl.create_vertex_buffer(error)) {
        shutdown();
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    if (!impl.create_depth_texture(static_cast<std::uint32_t>(width),
                                   static_cast<std::uint32_t>(height))) {
        error = std::string("depth texture creation failed: ") + SDL_GetError();
        shutdown();
        return false;
    }

    error.clear();
    return true;
}

void GpuRenderer::shutdown() {
    if (impl_ == nullptr) {
        return;
    }

    impl_->shutdown();
    if (impl_->device != nullptr) {
        SDL_DestroyGPUDevice(impl_->device);
        impl_->device = nullptr;
    }
    impl_.reset();
}

void GpuRenderer::resize(int width, int height) {
    if (impl_ == nullptr || width <= 0 || height <= 0) {
        return;
    }
    impl_->create_depth_texture(static_cast<std::uint32_t>(width),
                                static_cast<std::uint32_t>(height));
}

bool GpuRenderer::draw_frame(SDL_Window* window, float angle_radians, std::string& error) {
    if (impl_ == nullptr || impl_->device == nullptr || impl_->pipeline == nullptr) {
        error = "renderer is not initialized";
        return false;
    }

    Impl& impl = *impl_;

    SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(impl.device);
    if (command == nullptr) {
        error = std::string("SDL_AcquireGPUCommandBuffer failed: ") + SDL_GetError();
        return false;
    }

    SDL_GPUTexture* swapchain_texture = nullptr;
    Uint32 swapchain_width = 0;
    Uint32 swapchain_height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command,
                                               window,
                                               &swapchain_texture,
                                               &swapchain_width,
                                               &swapchain_height)) {
        error = std::string("SDL_WaitAndAcquireGPUSwapchainTexture failed: ") + SDL_GetError();
        SDL_CancelGPUCommandBuffer(command);
        return false;
    }

    if (swapchain_texture == nullptr) {
        // Minimized or otherwise unavailable; submit the empty command buffer.
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            error = std::string("SDL_SubmitGPUCommandBuffer failed: ") + SDL_GetError();
            return false;
        }
        return true;
    }

    if (swapchain_width != impl.width || swapchain_height != impl.height ||
        impl.depth_texture == nullptr) {
        if (!impl.create_depth_texture(swapchain_width, swapchain_height)) {
            error = std::string("depth texture recreation failed: ") + SDL_GetError();
            SDL_CancelGPUCommandBuffer(command);
            return false;
        }
    }

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = swapchain_texture;
    color_target.clear_color = SDL_FColor{0.04f, 0.05f, 0.07f, 1.0f};
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.cycle = true;

    SDL_GPUDepthStencilTargetInfo depth_target{};
    depth_target.texture = impl.depth_texture;
    depth_target.clear_depth = 1.0f;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_STORE;
    depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.cycle = true;

    SDL_GPURenderPass* render_pass =
        SDL_BeginGPURenderPass(command, &color_target, 1, &depth_target);
    if (render_pass == nullptr) {
        error = std::string("SDL_BeginGPURenderPass failed: ") + SDL_GetError();
        SDL_CancelGPUCommandBuffer(command);
        return false;
    }

    SDL_BindGPUGraphicsPipeline(render_pass, impl.pipeline);

    SDL_GPUViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = static_cast<float>(swapchain_width);
    viewport.h = static_cast<float>(swapchain_height);
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(render_pass, &viewport);

    SDL_GPUBufferBinding binding{};
    binding.buffer = impl.vertex_buffer;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);

    const float aspect = static_cast<float>(swapchain_width) /
                         static_cast<float>(swapchain_height);
    const Mat4 model = Mat4::rotation_y(angle_radians) * Mat4::rotation_x(angle_radians * 0.45f);
    const Mat4 view = Mat4::look_at(0.0f, 1.1f, 4.2f,
                                    0.0f, 0.0f, 0.0f,
                                    0.0f, 1.0f, 0.0f);
    const Mat4 projection = Mat4::perspective(1.0471975512f, aspect, 0.1f, 100.0f);
    const Mat4 mvp = projection * view * model;

    SDL_PushGPUVertexUniformData(command, 0, mvp.values.data(),
                                 static_cast<Uint32>(sizeof(float) * mvp.values.size()));

    SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(render_pass);

    if (!SDL_SubmitGPUCommandBuffer(command)) {
        error = std::string("SDL_SubmitGPUCommandBuffer failed: ") + SDL_GetError();
        return false;
    }

    ++impl.frames;
    error.clear();
    return true;
}

const GpuInfo& GpuRenderer::info() const noexcept {
    static const GpuInfo empty{};
    return impl_ != nullptr ? impl_->gpu_info : empty;
}

std::uint64_t GpuRenderer::frames_rendered() const noexcept {
    return impl_ != nullptr ? impl_->frames : 0;
}

std::uint32_t GpuRenderer::width() const noexcept {
    return impl_ != nullptr ? impl_->width : 0;
}

std::uint32_t GpuRenderer::height() const noexcept {
    return impl_ != nullptr ? impl_->height : 0;
}

} // namespace rr::render
