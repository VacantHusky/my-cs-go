#include "renderer/vulkan/MeshRuntime.h"

#include "util/FileSystem.h"

#include <spdlog/spdlog.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mycsg::renderer::vulkan {

namespace {

constexpr std::uint32_t kMeshMagic = 0x314D4353u;  // SCM1
constexpr std::uint32_t kMeshVersion = 2u;

std::string sanitizePathKey(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char character : value) {
        const unsigned char code = static_cast<unsigned char>(character);
        if ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') ||
            (code >= '0' && code <= '9') || character == '.' || character == '-' || character == '_') {
            out.push_back(character);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

std::filesystem::path normalizeRelativeToAssetRoot(const std::filesystem::path& assetRoot,
                                                   const std::filesystem::path& sourcePath) {
    if (sourcePath.empty()) {
        return {};
    }

    std::error_code error;
    if (!assetRoot.empty()) {
        if (sourcePath.is_absolute()) {
            const auto absoluteRelative = std::filesystem::relative(sourcePath, assetRoot, error);
            if (!error) {
                return absoluteRelative.lexically_normal();
            }
        } else {
            const auto normalizedSource = sourcePath.lexically_normal();
            const auto normalizedAssetRoot = assetRoot.lexically_normal();
            const auto sourceText = normalizedSource.generic_string();
            const auto assetRootText = normalizedAssetRoot.generic_string();
            if (sourceText == assetRootText) {
                return {};
            }
            if (!assetRootText.empty() &&
                sourceText.size() > assetRootText.size() &&
                sourceText.compare(0, assetRootText.size(), assetRootText) == 0 &&
                sourceText[assetRootText.size()] == '/') {
                return std::filesystem::path(sourceText.substr(assetRootText.size() + 1)).lexically_normal();
            }
            return normalizedSource;
        }
    }

    return sourcePath.lexically_normal();
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

template <typename T>
bool readValue(const std::vector<std::byte>& bytes, std::size_t& offset, T& value) {
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

util::Vec3 normalize(const util::Vec3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

util::Vec3 subtract(const util::Vec3 a, const util::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

util::Vec3 cross(const util::Vec3 a, const util::Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

void appendVertex(CpuMesh& mesh,
                  const util::Vec3 position,
                  const util::Vec3 normal,
                  const util::Vec2 uv,
                  const util::Vec3 color) {
    mesh.vertices.push_back({
        position.x, position.y, position.z,
        normal.x, normal.y, normal.z,
        uv.x, uv.y,
        color.x, color.y, color.z,
    });
}

void appendTriangle(CpuMesh& mesh,
                    const util::Vec3 a,
                    const util::Vec3 b,
                    const util::Vec3 c,
                    const util::Vec3 color) {
    const util::Vec3 normal = normalize(cross(subtract(b, a), subtract(c, a)));
    appendVertex(mesh, a, normal, {0.0f, 0.0f}, color);
    appendVertex(mesh, b, normal, {1.0f, 0.0f}, color);
    appendVertex(mesh, c, normal, {1.0f, 1.0f}, color);
}

void appendQuad(CpuMesh& mesh,
                const util::Vec3 a,
                const util::Vec3 b,
                const util::Vec3 c,
                const util::Vec3 d,
                const util::Vec3 color) {
    appendTriangle(mesh, a, b, c, color);
    appendTriangle(mesh, a, c, d, color);
}

util::Vec3 pathColor(const std::filesystem::path& sourcePath, const std::string& materialName = {}) {
    const std::string key = toLowerAscii(sourcePath.generic_string() + " " + materialName);
    if (key.find("barrel") != std::string::npos) {
        return {0.62f, 0.38f, 0.30f};
    }
    if (key.find("crate") != std::string::npos || key.find("wood") != std::string::npos) {
        return {0.66f, 0.50f, 0.32f};
    }
    if (key.find("knife") != std::string::npos || key.find("bayonet") != std::string::npos) {
        return {0.74f, 0.76f, 0.80f};
    }
    if (key.find("flash") != std::string::npos) {
        return {0.92f, 0.92f, 0.86f};
    }
    if (key.find("smoke") != std::string::npos) {
        return {0.60f, 0.64f, 0.68f};
    }
    if (key.find("grenade") != std::string::npos || key.find("frag") != std::string::npos) {
        return {0.34f, 0.44f, 0.30f};
    }
    if (key.find("sniper") != std::string::npos) {
        return {0.26f, 0.28f, 0.30f};
    }
    if (key.find("rifle") != std::string::npos || key.find("shotgun") != std::string::npos ||
        key.find("submachine") != std::string::npos || key.find("gun") != std::string::npos) {
        return {0.32f, 0.34f, 0.36f};
    }
    return {0.78f, 0.80f, 0.84f};
}

util::Vec3 blockColorForMaterial(const std::string& materialId, const bool shadedSide) {
    if (materialId.find("bomb_site_a") != std::string::npos) {
        return shadedSide ? util::Vec3{0.62f, 0.28f, 0.22f} : util::Vec3{0.78f, 0.34f, 0.28f};
    }
    if (materialId.find("bomb_site_b") != std::string::npos) {
        return shadedSide ? util::Vec3{0.26f, 0.40f, 0.66f} : util::Vec3{0.34f, 0.50f, 0.78f};
    }
    if (materialId.find("concrete") != std::string::npos) {
        return shadedSide ? util::Vec3{0.48f, 0.52f, 0.56f} : util::Vec3{0.62f, 0.66f, 0.70f};
    }
    return shadedSide ? util::Vec3{0.44f, 0.44f, 0.48f} : util::Vec3{0.58f, 0.58f, 0.62f};
}

void finalizeMeshBounds(CpuMesh& mesh) {
    if (mesh.vertices.empty()) {
        mesh.valid = false;
        return;
    }

    util::Vec3 min{mesh.vertices.front().px, mesh.vertices.front().py, mesh.vertices.front().pz};
    util::Vec3 max = min;
    for (const auto& vertex : mesh.vertices) {
        min.x = std::min(min.x, vertex.px);
        min.y = std::min(min.y, vertex.py);
        min.z = std::min(min.z, vertex.pz);
        max.x = std::max(max.x, vertex.px);
        max.y = std::max(max.y, vertex.py);
        max.z = std::max(max.z, vertex.pz);
    }

    mesh.center = {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    mesh.radius = 0.001f;
    for (const auto& vertex : mesh.vertices) {
        const float dx = vertex.px - mesh.center.x;
        const float dy = vertex.py - mesh.center.y;
        const float dz = vertex.pz - mesh.center.z;
        mesh.radius = std::max(mesh.radius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    mesh.valid = true;
}

util::Vec3 transformPosition(const float matrix[16], const util::Vec3 position) {
    return {
        matrix[0] * position.x + matrix[4] * position.y + matrix[8] * position.z + matrix[12],
        matrix[1] * position.x + matrix[5] * position.y + matrix[9] * position.z + matrix[13],
        matrix[2] * position.x + matrix[6] * position.y + matrix[10] * position.z + matrix[14],
    };
}

util::Vec3 transformDirection(const float matrix[16], const util::Vec3 direction) {
    return normalize({
        matrix[0] * direction.x + matrix[4] * direction.y + matrix[8] * direction.z,
        matrix[1] * direction.x + matrix[5] * direction.y + matrix[9] * direction.z,
        matrix[2] * direction.x + matrix[6] * direction.y + matrix[10] * direction.z,
    });
}

util::Vec2 readTexcoord(const tinyobj::attrib_t& attrib, const int texcoordIndex) {
    if (texcoordIndex < 0) {
        return {};
    }
    const std::size_t texcoordBase = static_cast<std::size_t>(texcoordIndex) * 2;
    if (texcoordBase + 1 >= attrib.texcoords.size()) {
        return {};
    }
    return {attrib.texcoords[texcoordBase], 1.0f - attrib.texcoords[texcoordBase + 1]};
}

util::Vec3 tinyObjMaterialColor(const tinyobj::material_t* material,
                                const std::filesystem::path& sourcePath,
                                const std::string& materialName) {
    if (material != nullptr) {
        return {
            std::clamp(material->diffuse[0], 0.0f, 1.0f),
            std::clamp(material->diffuse[1], 0.0f, 1.0f),
            std::clamp(material->diffuse[2], 0.0f, 1.0f),
        };
    }
    return pathColor(sourcePath, materialName);
}

CpuMesh loadObjSource(const std::filesystem::path& sourcePath) {
    CpuMesh mesh;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    const std::string filename = sourcePath.string();
    const std::string materialBaseDir = sourcePath.parent_path().string();
    const bool loaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        filename.c_str(), materialBaseDir.c_str(), true);

    if (!warn.empty()) {
        spdlog::warn("[MeshRuntime] tinyobjloader: {}", warn);
    }
    if (!loaded) {
        if (!err.empty()) {
            spdlog::warn("[MeshRuntime] Failed to parse OBJ {}: {}", sourcePath.generic_string(), err);
        }
        return mesh;
    }

    for (const auto& shape : shapes) {
        std::size_t indexOffset = 0;
        for (std::size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); ++faceIndex) {
            const int vertexCount = shape.mesh.num_face_vertices[faceIndex];
            if (vertexCount < 3) {
                indexOffset += static_cast<std::size_t>(std::max(vertexCount, 0));
                continue;
            }

            const int materialIndex = faceIndex < shape.mesh.material_ids.size() ? shape.mesh.material_ids[faceIndex] : -1;
            const tinyobj::material_t* material = (materialIndex >= 0 && materialIndex < static_cast<int>(materials.size()))
                ? &materials[static_cast<std::size_t>(materialIndex)]
                : nullptr;
            const std::string materialName = material != nullptr ? material->name : "";
            const util::Vec3 color = tinyObjMaterialColor(material, sourcePath, materialName);

            std::array<util::Vec3, 3> positions{};
            std::array<util::Vec3, 3> normals{};
            std::array<util::Vec2, 3> texcoords{};
            bool validTriangle = true;
            bool needsFallbackNormal = false;

            for (int vertex = 0; vertex < 3; ++vertex) {
                const tinyobj::index_t index = shape.mesh.indices[indexOffset + static_cast<std::size_t>(vertex)];
                if (index.vertex_index < 0) {
                    validTriangle = false;
                    break;
                }

                const std::size_t vertexBase = static_cast<std::size_t>(index.vertex_index) * 3;
                if (vertexBase + 2 >= attrib.vertices.size()) {
                    validTriangle = false;
                    break;
                }
                positions[vertex] = {
                    attrib.vertices[vertexBase],
                    attrib.vertices[vertexBase + 1],
                    attrib.vertices[vertexBase + 2],
                };
                texcoords[vertex] = readTexcoord(attrib, index.texcoord_index);

                if (index.normal_index >= 0) {
                    const std::size_t normalBase = static_cast<std::size_t>(index.normal_index) * 3;
                    if (normalBase + 2 < attrib.normals.size()) {
                        normals[vertex] = normalize({
                            attrib.normals[normalBase],
                            attrib.normals[normalBase + 1],
                            attrib.normals[normalBase + 2],
                        });
                        continue;
                    }
                }

                needsFallbackNormal = true;
                normals[vertex] = {};
            }

            if (!validTriangle) {
                indexOffset += static_cast<std::size_t>(std::max(vertexCount, 0));
                continue;
            }

            const util::Vec3 fallbackNormal = normalize(cross(
                subtract(positions[1], positions[0]),
                subtract(positions[2], positions[0])));

            for (int vertex = 0; vertex < 3; ++vertex) {
                appendVertex(mesh, positions[vertex], needsFallbackNormal ? fallbackNormal : normals[vertex], texcoords[vertex], color);
            }

            indexOffset += static_cast<std::size_t>(std::max(vertexCount, 0));
        }
    }

    finalizeMeshBounds(mesh);
    return mesh;
}

const cgltf_accessor* findPrimitiveAccessor(const cgltf_primitive& primitive, const cgltf_attribute_type type) {
    for (cgltf_size index = 0; index < primitive.attributes_count; ++index) {
        if (primitive.attributes[index].type == type) {
            return primitive.attributes[index].data;
        }
    }
    return nullptr;
}

util::Vec3 gltfMaterialColor(const std::filesystem::path& sourcePath, const cgltf_material* material) {
    if (material != nullptr && material->has_pbr_metallic_roughness) {
        const auto& factor = material->pbr_metallic_roughness.base_color_factor;
        if (factor[0] > 0.001f || factor[1] > 0.001f || factor[2] > 0.001f) {
            return {
                std::clamp(factor[0], 0.0f, 1.0f),
                std::clamp(factor[1], 0.0f, 1.0f),
                std::clamp(factor[2], 0.0f, 1.0f),
            };
        }
    }
    return pathColor(sourcePath, material != nullptr && material->name != nullptr ? material->name : "");
}

util::Vec3 readAccessorVec3(const cgltf_accessor* accessor, const cgltf_size index) {
    float values[4]{};
    if (accessor == nullptr || !cgltf_accessor_read_float(accessor, index, values, 3)) {
        return {};
    }
    return {values[0], values[1], values[2]};
}

util::Vec2 readAccessorVec2(const cgltf_accessor* accessor, const cgltf_size index) {
    float values[4]{};
    if (accessor == nullptr || !cgltf_accessor_read_float(accessor, index, values, 2)) {
        return {};
    }
    return {values[0], 1.0f - values[1]};
}

void appendGltfPrimitive(CpuMesh& mesh,
                         const std::filesystem::path& sourcePath,
                         const cgltf_node* node,
                         const cgltf_primitive& primitive) {
    if (primitive.type != cgltf_primitive_type_triangles) {
        return;
    }

    const cgltf_accessor* positionAccessor = findPrimitiveAccessor(primitive, cgltf_attribute_type_position);
    if (positionAccessor == nullptr) {
        return;
    }

    const cgltf_accessor* normalAccessor = findPrimitiveAccessor(primitive, cgltf_attribute_type_normal);
    const cgltf_accessor* texcoordAccessor = findPrimitiveAccessor(primitive, cgltf_attribute_type_texcoord);
    const util::Vec3 color = gltfMaterialColor(sourcePath, primitive.material);
    float matrix[16]{};
    cgltf_node_transform_world(node, matrix);

    const cgltf_size indexCount = primitive.indices != nullptr ? primitive.indices->count : positionAccessor->count;
    for (cgltf_size index = 0; index + 2 < indexCount; index += 3) {
        const std::array<cgltf_size, 3> triangleIndices{
            primitive.indices != nullptr ? cgltf_accessor_read_index(primitive.indices, index) : index,
            primitive.indices != nullptr ? cgltf_accessor_read_index(primitive.indices, index + 1) : index + 1,
            primitive.indices != nullptr ? cgltf_accessor_read_index(primitive.indices, index + 2) : index + 2,
        };

        if (triangleIndices[0] >= positionAccessor->count ||
            triangleIndices[1] >= positionAccessor->count ||
            triangleIndices[2] >= positionAccessor->count) {
            continue;
        }

        std::array<util::Vec3, 3> positions{};
        std::array<util::Vec3, 3> normals{};
        std::array<util::Vec2, 3> texcoords{};
        bool needsFallbackNormal = normalAccessor == nullptr;

        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            positions[vertex] = transformPosition(matrix, readAccessorVec3(positionAccessor, triangleIndices[vertex]));
            normals[vertex] = normalAccessor != nullptr
                ? transformDirection(matrix, readAccessorVec3(normalAccessor, triangleIndices[vertex]))
                : util::Vec3{};
            texcoords[vertex] = readAccessorVec2(texcoordAccessor, triangleIndices[vertex]);
            if (normalAccessor != nullptr) {
                const float lengthSquared =
                    normals[vertex].x * normals[vertex].x +
                    normals[vertex].y * normals[vertex].y +
                    normals[vertex].z * normals[vertex].z;
                if (lengthSquared <= 0.0001f) {
                    needsFallbackNormal = true;
                }
            }
        }

        const util::Vec3 fallbackNormal = normalize(cross(
            subtract(positions[1], positions[0]),
            subtract(positions[2], positions[0])));

        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            appendVertex(mesh, positions[vertex], needsFallbackNormal ? fallbackNormal : normals[vertex], texcoords[vertex], color);
        }
    }
}

void appendGltfNode(CpuMesh& mesh,
                    const std::filesystem::path& sourcePath,
                    const cgltf_node* node) {
    if (node->mesh != nullptr) {
        for (cgltf_size primitiveIndex = 0; primitiveIndex < node->mesh->primitives_count; ++primitiveIndex) {
            appendGltfPrimitive(mesh, sourcePath, node, node->mesh->primitives[primitiveIndex]);
        }
    }

    for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex) {
        appendGltfNode(mesh, sourcePath, node->children[childIndex]);
    }
}

CpuMesh loadGltfSource(const std::filesystem::path& sourcePath) {
    CpuMesh mesh;

    cgltf_options options{};
    cgltf_data* data = nullptr;
    const std::string filename = sourcePath.string();

    const cgltf_result parseResult = cgltf_parse_file(&options, filename.c_str(), &data);
    if (parseResult != cgltf_result_success || data == nullptr) {
        spdlog::warn("[MeshRuntime] Failed to parse glTF {} (code={}).", sourcePath.generic_string(), static_cast<int>(parseResult));
        return mesh;
    }

    const cgltf_result bufferResult = cgltf_load_buffers(&options, data, filename.c_str());
    if (bufferResult != cgltf_result_success) {
        spdlog::warn("[MeshRuntime] Failed to load glTF buffers {} (code={}).", sourcePath.generic_string(), static_cast<int>(bufferResult));
        cgltf_free(data);
        return mesh;
    }

    const cgltf_scene* scene = data->scene != nullptr
        ? data->scene
        : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
    if (scene == nullptr) {
        cgltf_free(data);
        return mesh;
    }

    for (cgltf_size nodeIndex = 0; nodeIndex < scene->nodes_count; ++nodeIndex) {
        appendGltfNode(mesh, sourcePath, scene->nodes[nodeIndex]);
    }

    cgltf_free(data);
    finalizeMeshBounds(mesh);
    return mesh;
}

}  // namespace

std::filesystem::path meshCachePathForSource(const std::filesystem::path& assetRoot,
                                             const std::filesystem::path& sourcePath) {
    std::filesystem::path relative = normalizeRelativeToAssetRoot(assetRoot, sourcePath);
    if (relative.empty()) {
        relative = sourcePath.filename();
    }

    const std::string key = sanitizePathKey(relative.generic_string());
    return assetRoot / "generated" / "mesh_cache" / (key + ".meshbin");
}

CpuMesh loadMeshFromCache(const std::filesystem::path& assetRoot,
                          const std::filesystem::path& sourcePath) {
    CpuMesh mesh;
    const std::filesystem::path cachePath = meshCachePathForSource(assetRoot, sourcePath);
    const std::vector<std::byte> bytes = util::FileSystem::readBinary(cachePath);
    if (bytes.empty()) {
        return mesh;
    }

    std::size_t offset = 0;
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t vertexCount = 0;
    if (!readValue(bytes, offset, magic) || !readValue(bytes, offset, version) || !readValue(bytes, offset, vertexCount)) {
        return mesh;
    }
    if (magic != kMeshMagic || version != kMeshVersion) {
        return mesh;
    }
    if (!readValue(bytes, offset, mesh.center.x) || !readValue(bytes, offset, mesh.center.y) ||
        !readValue(bytes, offset, mesh.center.z) || !readValue(bytes, offset, mesh.radius)) {
        return mesh;
    }

    mesh.vertices.resize(vertexCount);
    const std::size_t bytesNeeded = sizeof(MeshVertex) * mesh.vertices.size();
    if (offset + bytesNeeded > bytes.size()) {
        mesh.vertices.clear();
        return mesh;
    }
    std::memcpy(mesh.vertices.data(), bytes.data() + offset, bytesNeeded);
    mesh.valid = !mesh.vertices.empty();
    return mesh;
}

CpuMesh loadMeshFromSource(const std::filesystem::path& sourcePath) {
    if (!util::FileSystem::exists(sourcePath)) {
        return {};
    }

    const std::string extension = toLowerAscii(sourcePath.extension().string());
    if (extension == ".obj") {
        return loadObjSource(sourcePath);
    }
    if (extension == ".gltf") {
        return loadGltfSource(sourcePath);
    }
    return {};
}

CpuMesh loadMeshAsset(const std::filesystem::path& assetRoot,
                      const std::filesystem::path& sourcePath) {
    CpuMesh mesh = loadMeshFromCache(assetRoot, sourcePath);
    if (mesh.valid) {
        return mesh;
    }

    if (sourcePath.empty()) {
        return mesh;
    }

    spdlog::warn("[Renderer] Mesh cache missing, loading source asset directly: {}", sourcePath.generic_string());
    return loadMeshFromSource(sourcePath);
}

CpuMesh buildStaticWorldMesh(const gameplay::MapData& map) {
    CpuMesh mesh;

    for (int z = 0; z < map.depth; ++z) {
        for (int x = 0; x < map.width; ++x) {
            std::string floorMaterial = "floor_concrete";
            int wallHeight = 0;
            bool hasRamp = false;
            gameplay::RampDirection rampDirection = gameplay::RampDirection::North;

            for (const auto& block : map.blocks) {
                if (block.cell.x != x || block.cell.z != z) {
                    continue;
                }
                if (block.cell.y == 0) {
                    floorMaterial = block.materialId;
                }
                if (block.solid && block.cell.y >= 1) {
                    wallHeight = std::max(wallHeight, block.cell.y + 1);
                }
                if (gameplay::isRampMaterial(block.materialId)) {
                    hasRamp = true;
                    if (block.materialId.rfind("ramp_north", 0) == 0) rampDirection = gameplay::RampDirection::North;
                    else if (block.materialId.rfind("ramp_south", 0) == 0) rampDirection = gameplay::RampDirection::South;
                    else if (block.materialId.rfind("ramp_east", 0) == 0) rampDirection = gameplay::RampDirection::East;
                    else if (block.materialId.rfind("ramp_west", 0) == 0) rampDirection = gameplay::RampDirection::West;
                    floorMaterial = block.materialId;
                }
            }

            const util::Vec3 floorColor = blockColorForMaterial(floorMaterial, false);
            appendQuad(mesh,
                {static_cast<float>(x), 0.0f, static_cast<float>(z)},
                {static_cast<float>(x), 0.0f, static_cast<float>(z + 1)},
                {static_cast<float>(x + 1), 0.0f, static_cast<float>(z + 1)},
                {static_cast<float>(x + 1), 0.0f, static_cast<float>(z)},
                floorColor);

            if (hasRamp) {
                const util::Vec3 rampTop = blockColorForMaterial(floorMaterial, false);
                const util::Vec3 rampSide = blockColorForMaterial(floorMaterial, true);
                const float baseX = static_cast<float>(x);
                const float baseZ = static_cast<float>(z);
                const float low = 0.0f;
                const float high = 1.0f;
                if (rampDirection == gameplay::RampDirection::East) {
                    appendQuad(mesh, {baseX, low, baseZ}, {baseX, low, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ}, rampTop);
                    appendTriangle(mesh, {baseX, low, baseZ}, {baseX, low, baseZ + 1.0f}, {baseX, high, baseZ + 1.0f}, rampSide);
                    appendTriangle(mesh, {baseX, low, baseZ}, {baseX, high, baseZ + 1.0f}, {baseX, high, baseZ}, rampSide);
                } else if (rampDirection == gameplay::RampDirection::West) {
                    appendQuad(mesh, {baseX, high, baseZ}, {baseX, high, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ}, rampTop);
                    appendTriangle(mesh, {baseX + 1.0f, low, baseZ}, {baseX + 1.0f, high, baseZ}, {baseX + 1.0f, high, baseZ + 1.0f}, rampSide);
                    appendTriangle(mesh, {baseX + 1.0f, low, baseZ}, {baseX + 1.0f, high, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ + 1.0f}, rampSide);
                } else if (rampDirection == gameplay::RampDirection::South) {
                    appendQuad(mesh, {baseX, low, baseZ}, {baseX, high, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ}, rampTop);
                    appendTriangle(mesh, {baseX, low, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ + 1.0f}, rampSide);
                    appendTriangle(mesh, {baseX, low, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ + 1.0f}, {baseX, high, baseZ + 1.0f}, rampSide);
                } else {
                    appendQuad(mesh, {baseX, high, baseZ}, {baseX, low, baseZ + 1.0f}, {baseX + 1.0f, low, baseZ + 1.0f}, {baseX + 1.0f, high, baseZ}, rampTop);
                    appendTriangle(mesh, {baseX, low, baseZ}, {baseX + 1.0f, high, baseZ}, {baseX + 1.0f, low, baseZ}, rampSide);
                    appendTriangle(mesh, {baseX, low, baseZ}, {baseX, high, baseZ}, {baseX + 1.0f, high, baseZ}, rampSide);
                }
            }

            if (wallHeight <= 0) {
                continue;
            }

            const util::Vec3 topColor = blockColorForMaterial(floorMaterial, false);
            const util::Vec3 sideColor = blockColorForMaterial(floorMaterial, true);
            const float left = static_cast<float>(x);
            const float right = static_cast<float>(x + 1);
            const float front = static_cast<float>(z);
            const float back = static_cast<float>(z + 1);
            const float top = static_cast<float>(wallHeight);

            appendQuad(mesh, {left, top, front}, {left, top, back}, {right, top, back}, {right, top, front}, topColor);
            appendQuad(mesh, {left, 0.0f, front}, {left, top, front}, {right, top, front}, {right, 0.0f, front}, sideColor);
            appendQuad(mesh, {right, 0.0f, front}, {right, top, front}, {right, top, back}, {right, 0.0f, back}, sideColor);
            appendQuad(mesh, {left, 0.0f, front}, {left, 0.0f, back}, {left, top, back}, {left, top, front}, sideColor);
            appendQuad(mesh, {left, 0.0f, back}, {right, 0.0f, back}, {right, top, back}, {left, top, back}, sideColor);
        }
    }

    finalizeMeshBounds(mesh);
    return mesh;
}

}  // namespace mycsg::renderer::vulkan
