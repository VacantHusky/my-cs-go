cmake_minimum_required(VERSION 3.24)

project(my-cs-go VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MYCSGO_BUILD_TOOLS "Build utility tools" OFF)

set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_SSE4_1 OFF CACHE BOOL "" FORCE)
set(USE_SSE4_2 OFF CACHE BOOL "" FORCE)
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JoltPhysics/Build third_party/JoltPhysics/build)
add_subdirectory(third_party/enet)

add_library(mycsg_fastgltf STATIC
    third_party/fastgltf/src/base64.cpp
    third_party/fastgltf/src/fastgltf.cpp
    third_party/fastgltf/src/io.cpp
    third_party/simdjson/simdjson.cpp
)

add_library(mycsg_imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(mycsg_fastgltf SYSTEM PUBLIC
    third_party/fastgltf/include
    third_party/simdjson
)

target_include_directories(mycsg_imgui SYSTEM PUBLIC
    third_party/imgui
    third_party/imgui/backends
)

target_compile_definitions(mycsg_imgui PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    VK_NO_PROTOTYPES
)

target_compile_definitions(mycsg_fastgltf PUBLIC
    FASTGLTF_USE_CUSTOM_SMALLVECTOR=0
    FASTGLTF_ENABLE_DEPRECATED_EXT=0
    FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=0
    FASTGLTF_USE_64BIT_FLOAT=0
    FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES=0
    FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES=0
)

target_compile_definitions(mycsg_fastgltf PRIVATE
    SIMDJSON_TARGET_VERSION="3.12.3"
)

set(MYCSGO_APP_SOURCES
    src/app/Application.cpp
    src/app/Settings.cpp
    src/app/MainMenu.cpp
    src/audio/AudioSystem.cpp
    src/content/AssetManifest.cpp
    src/content/GameContent.cpp
    src/content/ObjectCatalog.cpp
    src/gameplay/MapData.cpp
    src/gameplay/PhysicsWorld.cpp
    src/gameplay/Simulation.cpp
    src/network/Network.cpp
    src/renderer/font/GeneratedUiFontData.cpp
    src/renderer/vulkan/MeshRuntime.cpp
    src/renderer/vulkan/VulkanRenderer.cpp
    src/util/FileSystem.cpp
    src/util/Log.cpp
)

set(MYCSGO_LOCAL_VULKAN_HEADERS "${CMAKE_SOURCE_DIR}/.local-tools/vendor/Vulkan-Headers-vulkan-sdk-1.4.341.0/include")
if(EXISTS "${MYCSGO_LOCAL_VULKAN_HEADERS}/vulkan/vulkan.h")
    set(MYCSGO_VULKAN_INCLUDE_DIR "${MYCSGO_LOCAL_VULKAN_HEADERS}")
else()
    find_path(MYCSGO_SYSTEM_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
    if(NOT MYCSGO_SYSTEM_VULKAN_INCLUDE_DIR)
        message(FATAL_ERROR "Vulkan headers were not found. Install Vulkan SDK or populate .local-tools/vendor/Vulkan-Headers-*/include.")
    endif()
    set(MYCSGO_VULKAN_INCLUDE_DIR "${MYCSGO_SYSTEM_VULKAN_INCLUDE_DIR}")
endif()

target_include_directories(mycsg_imgui PRIVATE "${MYCSGO_VULKAN_INCLUDE_DIR}")

if(WIN32)
    set(MYCSGO_SDL3_ROOT "${CMAKE_SOURCE_DIR}/third_party/SDL3/x86_64-w64-mingw32")
    if(EXISTS "${MYCSGO_SDL3_ROOT}/include/SDL3/SDL.h" AND EXISTS "${MYCSGO_SDL3_ROOT}/lib/libSDL3.dll.a")
        target_include_directories(mycsg_imgui SYSTEM PRIVATE "${MYCSGO_SDL3_ROOT}/include")
    else()
        message(FATAL_ERROR "SDL3 MinGW package was not found under third_party/SDL3/x86_64-w64-mingw32.")
    endif()

    set(MYCSGO_LLVM_MINGW_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/.local-tools/toolchains/llvm-mingw-20260311-ucrt-ubuntu-22.04-x86_64/x86_64-w64-mingw32/bin")
endif()

function(mycsg_configure_target target entry_source)
    add_executable(${target}
        ${entry_source}
        ${MYCSGO_APP_SOURCES}
    )

    target_link_libraries(${target} PRIVATE Jolt mycsg_fastgltf mycsg_imgui enet)

    target_include_directories(${target} PRIVATE
        src
        "${MYCSGO_VULKAN_INCLUDE_DIR}"
    )

    target_include_directories(${target} SYSTEM PRIVATE
        third_party/enet/include
        third_party/entt
        third_party/glm
        third_party/miniaudio
        third_party/spdlog/include
        third_party/stb
        third_party/tinyobjloader
        third_party/vma
    )

    if(WIN32)
        target_sources(${target} PRIVATE src/platform/win32/Win32Window.cpp)
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE VK_USE_PLATFORM_WIN32_KHR)
        target_include_directories(${target} SYSTEM PRIVATE "${MYCSGO_SDL3_ROOT}/include")
        target_link_libraries(${target} PRIVATE "${MYCSGO_SDL3_ROOT}/lib/libSDL3.dll.a")
        target_link_libraries(${target} PRIVATE user32 gdi32 ws2_32 winmm ole32 avrt)

        if(EXISTS "${MYCSGO_SDL3_ROOT}/bin/SDL3.dll")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${MYCSGO_SDL3_ROOT}/bin/SDL3.dll"
                    "$<TARGET_FILE_DIR:${target}>/SDL3.dll"
                VERBATIM
            )
        endif()

        if(EXISTS "${MYCSGO_LLVM_MINGW_RUNTIME_DIR}")
            foreach(MYCSGO_RUNTIME_DLL IN ITEMS libc++.dll libunwind.dll libwinpthread-1.dll)
                if(EXISTS "${MYCSGO_LLVM_MINGW_RUNTIME_DIR}/${MYCSGO_RUNTIME_DLL}")
                    add_custom_command(TARGET ${target} POST_BUILD
                        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                            "${MYCSGO_LLVM_MINGW_RUNTIME_DIR}/${MYCSGO_RUNTIME_DLL}"
                            "$<TARGET_FILE_DIR:${target}>/${MYCSGO_RUNTIME_DLL}"
                        VERBATIM
                    )
                endif()
            endforeach()
        endif()
    else()
        target_sources(${target} PRIVATE src/platform/null/NullWindow.cpp)
        target_link_libraries(${target} PRIVATE dl)
    endif()

    target_compile_definitions(${target} PRIVATE
        MYCSGO_PROJECT_NAME="${target}"
        MYCSGO_ASSET_ROOT="${CMAKE_SOURCE_DIR}/assets"
        VK_NO_PROTOTYPES
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_property(TARGET ${target} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

mycsg_configure_target(my-cs-go src/main.cpp)
mycsg_configure_target(my-cs-go-editor src/editor_main.cpp)
