#!/usr/bin/env sh
# Regenerate the TASK_001 Stage C SPIR-V test shaders.
#
# These are known-good precompiled Vulkan/SPIR-V artifacts for the Linux
# SDL_GPU smoke test. The source files are the authority.
#
# Verified generation tool:
#   glslang 11:16.2.0 (Ubuntu package glslang-tools 16.2.0-2)
#   glslangValidator equivalent from Khronos glslang
#
# Usage:
#   GLSLANG=/path/to/glslang ./compile_shaders.sh
#
# Other SDL_GPU backends (DXIL for D3D12, MSL for Metal) are not produced by
# this script. They require the pinned SDL_shadercross offline pipeline when
# those platform stages are implemented and tested.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GLSLANG=${GLSLANG:-glslang}

"$GLSLANG" -V --target-env vulkan1.1 "$SCRIPT_DIR/triangle.vert" \
    -o "$SCRIPT_DIR/triangle.vert.spv"
"$GLSLANG" -V --target-env vulkan1.1 "$SCRIPT_DIR/triangle.frag" \
    -o "$SCRIPT_DIR/triangle.frag.spv"

echo "Regenerated:"
sha256sum "$SCRIPT_DIR/triangle.vert" \
          "$SCRIPT_DIR/triangle.frag" \
          "$SCRIPT_DIR/triangle.vert.spv" \
          "$SCRIPT_DIR/triangle.frag.spv"
