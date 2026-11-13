#version 450

// RustyRage TASK_001 Stage C test shader.
// SDL_GPU SPIR-V convention: vertex uniform buffers live in set 1.
layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 mvp;
} camera;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 frag_color;

void main() {
    frag_color = in_color;
    gl_Position = camera.mvp * vec4(in_position, 1.0);
}
