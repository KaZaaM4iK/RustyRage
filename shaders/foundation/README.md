# Foundation test shaders (TASK_001 Stage C)

Source:

- `triangle.vert` — vertex shader with a `mat4` camera MVP uniform.
- `triangle.frag` — flat fragment color.

Precompiled artifacts:

- `triangle.vert.spv`
- `triangle.frag.spv`

The artifacts are Vulkan/SPIR-V for the Linux SDL_GPU Vulkan smoke test. They
were generated offline, not at runtime, with:

```text
glslang 11:16.2.0 (Ubuntu glslang-tools 16.2.0-2)
glslang -V --target-env vulkan1.1 triangle.vert -o triangle.vert.spv
glslang -V --target-env vulkan1.1 triangle.frag -o triangle.frag.spv
```

Regenerate with `./compile_shaders.sh` (set `GLSLANG` when the tool is not on
`PATH`). SHA-256 values recorded at generation:

```text
13133d2641bbedbc999e38ab5294b00d276607ca82c9594687f0c7fe90ed1cae  triangle.frag
e83614d66958a3037374edfcb025cecf8caf00d74150d9c4970f81e28754d37e  triangle.frag.spv
cdcb833eaf932140c625e0e95fe1c362a11133d54b8e97522b4eadf28d20ed04  triangle.vert
0ea6b36b8caed4c93d66e4a10b20a9be2102c2e7dec3a79a90b9ecd2888007f1  triangle.vert.spv
```

SDL_GPU SPIR-V binding convention used by this test:

- vertex uniform buffer set 1, binding 0 (one uniform buffer);
- fragment shader uses no samplers, storage resources or uniform buffers.

The client passes the camera MVP through `SDL_PushGPUVertexUniformData` slot 0.

For Windows D3D12 (DXIL) or macOS/iOS Metal (MSL), these SPIR-V artifacts are
not sufficient. Those backends require regeneration through the pinned
`SDL_shadercross` offline pipeline when the corresponding platform stage is
implemented and tested. Do not represent this Linux SPIR-V artifact as a
cross-backend shader pass.
