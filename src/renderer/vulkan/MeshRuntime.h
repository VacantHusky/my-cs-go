#pragma once

#include "gameplay/MapData.h"
#include "util/MathTypes.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace mycsg::renderer::vulkan {

struct MeshVertex {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct CpuMesh {
    std::vector<MeshVertex> vertices;
    util::Vec3 center{};
    float radius = 1.0f;
    bool valid = false;
};

std::filesystem::path meshCachePathForSource(const std::filesystem::path& assetRoot,
                                             const std::filesystem::path& sourcePath);

CpuMesh loadMeshFromCache(const std::filesystem::path& assetRoot,
                          const std::filesystem::path& sourcePath);

CpuMesh loadMeshFromSource(const std::filesystem::path& sourcePath);

CpuMesh loadMeshAsset(const std::filesystem::path& assetRoot,
                      const std::filesystem::path& sourcePath);

CpuMesh buildStaticWorldMesh(const gameplay::MapData& map);

}  // namespace mycsg::renderer::vulkan
