# Vendored Dependencies

This project vendors a small set of source dependencies directly into `third_party/`
so Windows cross-builds do not depend on system package managers.

Current dependencies:

- `glm` `1.0.3`
  Source: upstream release tarball from `g-truc/glm`
  Usage: math types and matrix transforms for Vulkan camera/model/projection work.

- `spdlog` `1.17.0`
  Source: upstream release tarball from `gabime/spdlog`
  Usage: unified structured console logging across app/bootstrap/renderer code.

- `tinyobjloader` `release` branch snapshot downloaded on `2026-03-24`
  Source: `https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h`
  Usage: runtime OBJ source-mesh fallback when generated mesh cache is missing.

- `cgltf` `v1.15`
  Source: `https://raw.githubusercontent.com/jkuhlmann/cgltf/v1.15/cgltf.h`
  Usage: runtime glTF source-mesh fallback when generated mesh cache is missing.

- `stb_image` `master` snapshot downloaded on `2026-03-24`
  Source: `https://raw.githubusercontent.com/nothings/stb/master/stb_image.h`
  Usage: decode JPG/PPM texture assets into RGBA pixels for Vulkan albedo uploads.

- `Vulkan Memory Allocator (VMA)` `master` snapshot downloaded on `2026-03-24`
  Source: `https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h`
  Usage: unified Vulkan buffer/image allocation for vertex buffers, staging buffers,
  textures, and depth resources.

- `SDL3` `3.4.0`
  Source: official MinGW development package from `libsdl-org/SDL` releases
  Usage: Windows window creation, event/input handling, relative mouse mode,
  and native window interop while keeping the Vulkan renderer.

Notes:

- `tinyobjloader` and `cgltf` are single-header dependencies; their upstream license
  text is included in the vendored header comments.
- `stb_image` and `VMA` are also vendored as single-header libraries with upstream
  license text preserved in the header comments.
- `SDL3` is vendored as an official prebuilt MinGW package so Windows cross-builds
  can link and ship `SDL3.dll` without requiring a separate local installation.
- The preferred runtime path is still the generated binary mesh cache under
  `assets/generated/mesh_cache/`; these libraries are used as a resilient fallback
  so new or swapped assets can still appear before cache regeneration.
