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

- `fastgltf` `v0.9.0`
  Source: upstream release tarball from `spnda/fastgltf`
  Usage: runtime glTF/glb source-mesh fallback with a more complete accessor/node/material
  model and a more production-like loading path.

- `EnTT` `v3.15.0`
  Source: `https://raw.githubusercontent.com/skypjack/entt/v3.15.0/single_include/entt/entt.hpp`
  Usage: ECS registry, entity/component storage, and system-friendly gameplay state modeling
  for players and future projectile/vehicle/AI expansion.

- `Jolt Physics` `v5.4.0`
  Source: upstream release source tree from `jrouwe/JoltPhysics`
  Usage: character movement, collision world, ray casts, throwable rigid bodies,
  and a scalable physics base for future stairs/slopes/vehicles.

- `miniaudio` `0.11.23`
  Source: official single-header release from `mackron/miniaudio`
  Usage: lightweight cross-platform audio output for UI cues, weapon shots,
  explosions, jump/reload feedback, and future streamed sound assets.

- `simdjson` `v3.12.3`
  Source: `https://raw.githubusercontent.com/simdjson/simdjson/v3.12.3/singleheader/simdjson.h`
  and `https://raw.githubusercontent.com/simdjson/simdjson/v3.12.3/singleheader/simdjson.cpp`
  Usage: vendored JSON backend required by `fastgltf`.

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

- `ENet` commit `8be2368`
  Source: upstream source tree from `lsalzman/enet`
  Usage: reliable/unreliable UDP session transport for host/client multiplayer,
  peer connection management, and a faster path to iterate on room-based online play.

- `Dear ImGui` commit `79411a0`
  Source: upstream source tree from `ocornut/imgui`
  Usage: immediate-mode tooling UI for future Chinese menus, map editor panels,
  and faster iteration on in-engine developer tools over the Vulkan renderer.

Notes:

- `tinyobjloader` remains a single-header dependency; its upstream license text is
  included in the vendored header comments.
- `EnTT` is vendored as its official single-header distribution to keep ECS integration
  simple and cross-platform.
- `Jolt Physics` is vendored as the upstream source tree and built through its official
  CMake target so physics updates stay close to upstream instead of maintaining a custom
  file list by hand.
- `fastgltf` is vendored as source plus headers, with `simdjson` vendored alongside it
  to keep Windows cross-builds offline and reproducible.
- `miniaudio` is vendored as a single-header dependency and currently drives a small
  synthesized effects path so the packaged build does not depend on external audio files.
- `stb_image` and `VMA` are also vendored as single-header libraries with upstream
  license text preserved in the header comments.
- `SDL3` is vendored as an official prebuilt MinGW package so Windows cross-builds
  can link and ship `SDL3.dll` without requiring a separate local installation.
- `ENet` is vendored as the upstream source tree and built as a static library so
  the networking layer can use peer/session semantics without introducing another
  runtime DLL into the Windows package.
- `Dear ImGui` is vendored with its SDL3 and Vulkan backends so the existing
  window/input stack can grow into a more capable editor UI without replacing
  the whole renderer or abandoning the current packaged Windows workflow.
- The preferred runtime path is still the generated binary mesh cache under
  `assets/generated/mesh_cache/`; these libraries are used as a resilient fallback
  so new or swapped assets can still appear before cache regeneration.
