#include "renderer/vulkan/MeshRuntime.h"

#include "util/FileSystem.h"

#include <spdlog/spdlog.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
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

[[maybe_unused]] void appendTriangle(CpuMesh& mesh,
                                     const util::Vec3 a,
                                     const util::Vec3 b,
                                     const util::Vec3 c,
                                     const util::Vec3 color) {
    const util::Vec3 normal = normalize(cross(subtract(b, a), subtract(c, a)));
    appendVertex(mesh, a, normal, {0.0f, 0.0f}, color);
    appendVertex(mesh, b, normal, {1.0f, 0.0f}, color);
    appendVertex(mesh, c, normal, {1.0f, 1.0f}, color);
}

[[maybe_unused]] void appendQuad(CpuMesh& mesh,
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

[[maybe_unused]] util::Vec3 blockColorForMaterial(const std::string& materialId, const bool shadedSide) {
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

util::Vec3 transformPosition(const fastgltf::math::fmat4x4& matrix,
                             const fastgltf::math::fvec3 position) {
    const fastgltf::math::fvec4 transformed = matrix * fastgltf::math::fvec4(
        position.x(),
        position.y(),
        position.z(),
        1.0f);
    return {transformed.x(), transformed.y(), transformed.z()};
}

util::Vec3 transformDirection(const fastgltf::math::fmat4x4& matrix,
                              const fastgltf::math::fvec3 direction) {
    const fastgltf::math::fvec4 transformed = matrix * fastgltf::math::fvec4(
        direction.x(),
        direction.y(),
        direction.z(),
        0.0f);
    return normalize({transformed.x(), transformed.y(), transformed.z()});
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

const fastgltf::Accessor* findPrimitiveAccessor(const fastgltf::Asset& asset,
                                                const fastgltf::Primitive& primitive,
                                                const std::string_view attributeName) {
    const auto accessorIt = primitive.findAttribute(attributeName);
    if (accessorIt == primitive.attributes.cend()) {
        return nullptr;
    }
    if (accessorIt->accessorIndex >= asset.accessors.size()) {
        return nullptr;
    }
    return &asset.accessors[accessorIt->accessorIndex];
}

util::Vec3 gltfMaterialColor(const std::filesystem::path& sourcePath,
                             const fastgltf::Asset& asset,
                             const fastgltf::Primitive& primitive) {
    if (primitive.materialIndex && *primitive.materialIndex < asset.materials.size()) {
        const auto& material = asset.materials[*primitive.materialIndex];
        const auto& factor = material.pbrData.baseColorFactor;
        const bool isDefaultWhite =
            std::abs(factor.x() - 1.0f) <= 0.01f &&
            std::abs(factor.y() - 1.0f) <= 0.01f &&
            std::abs(factor.z() - 1.0f) <= 0.01f;
        if (!isDefaultWhite && (factor.x() > 0.001f || factor.y() > 0.001f || factor.z() > 0.001f)) {
            return {
                std::clamp(factor.x(), 0.0f, 1.0f),
                std::clamp(factor.y(), 0.0f, 1.0f),
                std::clamp(factor.z(), 0.0f, 1.0f),
            };
        }
        return pathColor(sourcePath, std::string(material.name));
    }
    return pathColor(sourcePath);
}

struct DecodedGltfImage {
    std::vector<unsigned char> rgba;
    int width = 0;
    int height = 0;
    bool attempted = false;
    bool valid = false;
};

struct PrimitiveColorSource {
    util::Vec3 fallbackColor{1.0f, 1.0f, 1.0f};
    util::Vec3 factor{1.0f, 1.0f, 1.0f};
    const DecodedGltfImage* image = nullptr;
    std::size_t texCoordSet = 0;
    fastgltf::Wrap wrapS = fastgltf::Wrap::Repeat;
    fastgltf::Wrap wrapT = fastgltf::Wrap::Repeat;
};

std::vector<std::byte> copyBytes(const std::span<const std::byte> bytes) {
    return {bytes.begin(), bytes.end()};
}

std::vector<std::byte> loadGltfImageBytes(const fastgltf::Asset& asset,
                                          const fastgltf::Image& image,
                                          const std::filesystem::path& sourcePath) {
    if (const auto* bufferViewSource = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        const fastgltf::DefaultBufferDataAdapter adapter;
        return copyBytes(adapter(asset, bufferViewSource->bufferViewIndex));
    }
    if (const auto* arraySource = std::get_if<fastgltf::sources::Array>(&image.data)) {
        return copyBytes(std::span<const std::byte>(arraySource->bytes.data(), arraySource->bytes.size()));
    }
    if (const auto* vectorSource = std::get_if<fastgltf::sources::Vector>(&image.data)) {
        return {vectorSource->bytes.begin(), vectorSource->bytes.end()};
    }
    if (const auto* byteViewSource = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
        return copyBytes(byteViewSource->bytes);
    }
    if (const auto* uriSource = std::get_if<fastgltf::sources::URI>(&image.data)) {
        if (uriSource->uri.isLocalPath()) {
            return util::FileSystem::readBinary((sourcePath.parent_path() / uriSource->uri.fspath()).lexically_normal());
        }
    }
    return {};
}

const DecodedGltfImage* ensureDecodedGltfImage(std::vector<DecodedGltfImage>& cache,
                                               const fastgltf::Asset& asset,
                                               const std::filesystem::path& sourcePath,
                                               const std::size_t imageIndex) {
    if (imageIndex >= asset.images.size()) {
        return nullptr;
    }

    DecodedGltfImage& decoded = cache[imageIndex];
    if (!decoded.attempted) {
        decoded.attempted = true;
        const std::vector<std::byte> bytes = loadGltfImageBytes(asset, asset.images[imageIndex], sourcePath);
        if (!bytes.empty()) {
            int width = 0;
            int height = 0;
            int channels = 0;
            unsigned char* pixels = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(bytes.data()),
                static_cast<int>(bytes.size()),
                &width,
                &height,
                &channels,
                STBI_rgb_alpha);
            if (pixels != nullptr && width > 0 && height > 0) {
                decoded.width = width;
                decoded.height = height;
                decoded.rgba.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
                decoded.valid = true;
            }
            stbi_image_free(pixels);
        }
    }

    return decoded.valid ? &decoded : nullptr;
}

float wrapUv(const float uv, const fastgltf::Wrap wrapMode) {
    switch (wrapMode) {
        case fastgltf::Wrap::ClampToEdge:
            return std::clamp(uv, 0.0f, 1.0f);
        case fastgltf::Wrap::MirroredRepeat: {
            float wrapped = std::fmod(uv, 2.0f);
            if (wrapped < 0.0f) {
                wrapped += 2.0f;
            }
            return wrapped <= 1.0f ? wrapped : (2.0f - wrapped);
        }
        case fastgltf::Wrap::Repeat:
        default: {
            float wrapped = std::fmod(uv, 1.0f);
            if (wrapped < 0.0f) {
                wrapped += 1.0f;
            }
            return wrapped;
        }
    }
}

util::Vec3 sampleDecodedGltfImage(const DecodedGltfImage& image,
                                  const util::Vec2 uv,
                                  const fastgltf::Wrap wrapS,
                                  const fastgltf::Wrap wrapT) {
    if (!image.valid || image.width <= 0 || image.height <= 0) {
        return {1.0f, 1.0f, 1.0f};
    }

    const float u = wrapUv(uv.x, wrapS);
    const float v = wrapUv(uv.y, wrapT);
    const int pixelX = std::clamp(static_cast<int>(std::round(u * static_cast<float>(image.width - 1))), 0, image.width - 1);
    const int pixelY = std::clamp(static_cast<int>(std::round(v * static_cast<float>(image.height - 1))), 0, image.height - 1);
    const std::size_t index = static_cast<std::size_t>((pixelY * image.width + pixelX) * 4);
    if (index + 2 >= image.rgba.size()) {
        return {1.0f, 1.0f, 1.0f};
    }

    return {
        static_cast<float>(image.rgba[index + 0]) / 255.0f,
        static_cast<float>(image.rgba[index + 1]) / 255.0f,
        static_cast<float>(image.rgba[index + 2]) / 255.0f,
    };
}

const fastgltf::Accessor* findTexcoordAccessor(const fastgltf::Asset& asset,
                                               const fastgltf::Primitive& primitive,
                                               const std::size_t texCoordSet) {
    const std::string attributeName = "TEXCOORD_" + std::to_string(texCoordSet);
    return findPrimitiveAccessor(asset, primitive, attributeName);
}

PrimitiveColorSource buildPrimitiveColorSource(const std::filesystem::path& sourcePath,
                                               const fastgltf::Asset& asset,
                                               const fastgltf::Primitive& primitive,
                                               std::vector<DecodedGltfImage>& imageCache) {
    PrimitiveColorSource source{};
    source.fallbackColor = gltfMaterialColor(sourcePath, asset, primitive);

    if (!primitive.materialIndex || *primitive.materialIndex >= asset.materials.size()) {
        return source;
    }

    const auto& material = asset.materials[*primitive.materialIndex];
    const auto& factor = material.pbrData.baseColorFactor;
    source.factor = {
        std::clamp(factor.x(), 0.0f, 1.0f),
        std::clamp(factor.y(), 0.0f, 1.0f),
        std::clamp(factor.z(), 0.0f, 1.0f),
    };

    if (!material.pbrData.baseColorTexture) {
        return source;
    }

    const std::size_t textureIndex = material.pbrData.baseColorTexture->textureIndex;
    if (textureIndex >= asset.textures.size()) {
        return source;
    }

    source.texCoordSet = material.pbrData.baseColorTexture->texCoordIndex;
    const auto& texture = asset.textures[textureIndex];
    if (texture.samplerIndex && *texture.samplerIndex < asset.samplers.size()) {
        const auto& sampler = asset.samplers[*texture.samplerIndex];
        source.wrapS = sampler.wrapS;
        source.wrapT = sampler.wrapT;
    }

    if (!texture.imageIndex || *texture.imageIndex >= asset.images.size()) {
        return source;
    }

    source.image = ensureDecodedGltfImage(imageCache, asset, sourcePath, *texture.imageIndex);
    return source;
}

void appendGltfPrimitive(CpuMesh& mesh,
                         const std::filesystem::path& sourcePath,
                         const fastgltf::Asset& asset,
                         std::vector<DecodedGltfImage>& imageCache,
                         const fastgltf::math::fmat4x4& transform,
                         const fastgltf::Primitive& primitive) {
    if (primitive.type != fastgltf::PrimitiveType::Triangles) {
        return;
    }

    const fastgltf::Accessor* positionAccessor = findPrimitiveAccessor(asset, primitive, "POSITION");
    if (positionAccessor == nullptr || positionAccessor->type != fastgltf::AccessorType::Vec3) {
        return;
    }

    const fastgltf::Accessor* normalAccessor = findPrimitiveAccessor(asset, primitive, "NORMAL");
    const PrimitiveColorSource colorSource = buildPrimitiveColorSource(sourcePath, asset, primitive, imageCache);
    const fastgltf::Accessor* texcoordAccessor = findTexcoordAccessor(asset, primitive, colorSource.texCoordSet);

    const std::size_t vertexCount = positionAccessor->count;
    if (vertexCount == 0) {
        return;
    }

    std::vector<util::Vec3> positions(vertexCount);
    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, *positionAccessor,
        [&](const fastgltf::math::fvec3 value, const std::size_t index) {
            if (index < positions.size()) {
                positions[index] = transformPosition(transform, value);
            }
        });

    std::vector<util::Vec3> normals(vertexCount);
    bool hasNormals = normalAccessor != nullptr && normalAccessor->type == fastgltf::AccessorType::Vec3;
    if (hasNormals) {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, *normalAccessor,
            [&](const fastgltf::math::fvec3 value, const std::size_t index) {
                if (index < normals.size()) {
                    normals[index] = transformDirection(transform, value);
                }
            });
    }

    std::vector<util::Vec2> texcoords(vertexCount);
    const bool hasTexcoords = texcoordAccessor != nullptr && texcoordAccessor->type == fastgltf::AccessorType::Vec2;
    if (hasTexcoords) {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, *texcoordAccessor,
            [&](const fastgltf::math::fvec2 value, const std::size_t index) {
                if (index < texcoords.size()) {
                    texcoords[index] = {value.x(), 1.0f - value.y()};
                }
            });
    }

    std::vector<std::uint32_t> indices;
    if (primitive.indicesAccessor && *primitive.indicesAccessor < asset.accessors.size()) {
        const fastgltf::Accessor& indexAccessor = asset.accessors[*primitive.indicesAccessor];
        if (indexAccessor.type != fastgltf::AccessorType::Scalar || indexAccessor.count < 3) {
            return;
        }
        indices.resize(indexAccessor.count);
        fastgltf::copyFromAccessor<std::uint32_t>(asset, indexAccessor, indices.data());
    } else {
        indices.resize(vertexCount);
        for (std::size_t index = 0; index < vertexCount; ++index) {
            indices[index] = static_cast<std::uint32_t>(index);
        }
    }

    for (std::size_t index = 0; index + 2 < indices.size(); index += 3) {
        const std::array<std::size_t, 3> triangleIndices{
            static_cast<std::size_t>(indices[index]),
            static_cast<std::size_t>(indices[index + 1]),
            static_cast<std::size_t>(indices[index + 2]),
        };

        if (triangleIndices[0] >= positions.size() ||
            triangleIndices[1] >= positions.size() ||
            triangleIndices[2] >= positions.size()) {
            continue;
        }

        std::array<util::Vec3, 3> trianglePositions{};
        std::array<util::Vec3, 3> triangleNormals{};
        std::array<util::Vec2, 3> triangleTexcoords{};
        bool needsFallbackNormal = !hasNormals;

        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            trianglePositions[vertex] = positions[triangleIndices[vertex]];
            triangleNormals[vertex] = hasNormals ? normals[triangleIndices[vertex]] : util::Vec3{};
            triangleTexcoords[vertex] = hasTexcoords ? texcoords[triangleIndices[vertex]] : util::Vec2{};
            if (hasNormals) {
                const float lengthSquared =
                    triangleNormals[vertex].x * triangleNormals[vertex].x +
                    triangleNormals[vertex].y * triangleNormals[vertex].y +
                    triangleNormals[vertex].z * triangleNormals[vertex].z;
                if (lengthSquared <= 0.0001f) {
                    needsFallbackNormal = true;
                }
            }
        }

        const util::Vec3 fallbackNormal = normalize(cross(
            subtract(trianglePositions[1], trianglePositions[0]),
            subtract(trianglePositions[2], trianglePositions[0])));

        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            util::Vec3 color = colorSource.fallbackColor;
            if (colorSource.image != nullptr && hasTexcoords) {
                const util::Vec3 albedo = sampleDecodedGltfImage(*colorSource.image,
                                                                 triangleTexcoords[vertex],
                                                                 colorSource.wrapS,
                                                                 colorSource.wrapT);
                color = {
                    std::clamp(albedo.x * colorSource.factor.x, 0.0f, 1.0f),
                    std::clamp(albedo.y * colorSource.factor.y, 0.0f, 1.0f),
                    std::clamp(albedo.z * colorSource.factor.z, 0.0f, 1.0f),
                };
            }
            appendVertex(mesh,
                         trianglePositions[vertex],
                         needsFallbackNormal ? fallbackNormal : triangleNormals[vertex],
                         triangleTexcoords[vertex],
                         color);
        }
    }
}

CpuMesh loadGltfSource(const std::filesystem::path& sourcePath) {
    CpuMesh mesh;

    auto buffer = fastgltf::GltfDataBuffer::FromPath(sourcePath);
    if (buffer.error() != fastgltf::Error::None) {
        spdlog::warn("[MeshRuntime] Failed to open glTF {} ({}: {}).",
                     sourcePath.generic_string(),
                     fastgltf::getErrorName(buffer.error()),
                     fastgltf::getErrorMessage(buffer.error()));
        return mesh;
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
    constexpr fastgltf::Options kLoadOptions =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices;

    auto assetResult = parser.loadGltf(buffer.get(), sourcePath.parent_path(), kLoadOptions);
    if (assetResult.error() != fastgltf::Error::None) {
        spdlog::warn("[MeshRuntime] Failed to parse glTF {} ({}: {}).",
                     sourcePath.generic_string(),
                     fastgltf::getErrorName(assetResult.error()),
                     fastgltf::getErrorMessage(assetResult.error()));
        return mesh;
    }

    fastgltf::Asset& asset = assetResult.get();
    if (asset.scenes.empty()) {
        spdlog::warn("[MeshRuntime] glTF scene list is empty: {}", sourcePath.generic_string());
        return mesh;
    }

    std::size_t sceneIndex = asset.defaultScene.value_or(0);
    if (sceneIndex >= asset.scenes.size()) {
        sceneIndex = 0;
    }

    std::vector<DecodedGltfImage> imageCache(asset.images.size());
    fastgltf::iterateSceneNodes(asset, sceneIndex, fastgltf::math::fmat4x4(),
        [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& transform) {
            if (!node.meshIndex || *node.meshIndex >= asset.meshes.size()) {
                return;
            }

            const fastgltf::Mesh& gltfMesh = asset.meshes[*node.meshIndex];
            for (const auto& primitive : gltfMesh.primitives) {
                appendGltfPrimitive(mesh, sourcePath, asset, imageCache, transform, primitive);
            }
        });

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
    if (extension == ".gltf" || extension == ".glb") {
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
    (void)map;
    return {};
}

}  // namespace mycsg::renderer::vulkan
