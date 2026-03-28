#include "content/AssetManifest.h"

#include "renderer/vulkan/MeshRuntime.h"
#include "util/FileSystem.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>

namespace mycsg::content {

namespace {

constexpr std::string_view kManifestFilePath = "generated/asset_manifest.json";
constexpr std::string_view kAutoMaterialRoot = "generated/materials/manifest";
constexpr std::string_view kThumbnailRoot = "generated/thumbnails/manifest";

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trimAscii(std::string value) {
    const auto isSpace = [](const unsigned char character) {
        return std::isspace(character) != 0;
    };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string prettifyToken(std::string value) {
    for (char& character : value) {
        if (character == '_' || character == '-' || character == '[' || character == ']' || character == '(' || character == ')') {
            character = ' ';
        }
    }

    std::string result;
    result.reserve(value.size());
    bool previousWasSpace = true;
    for (const unsigned char character : value) {
        if (std::isspace(character) != 0) {
            if (!previousWasSpace) {
                result.push_back(' ');
            }
            previousWasSpace = true;
            continue;
        }
        result.push_back(static_cast<char>(character));
        previousWasSpace = false;
    }
    return trimAscii(result);
}

std::string sanitizeId(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    bool previousWasUnderscore = false;
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            result.push_back(static_cast<char>(std::tolower(character)));
            previousWasUnderscore = false;
        } else if (!previousWasUnderscore) {
            result.push_back('_');
            previousWasUnderscore = true;
        }
    }
    while (!result.empty() && result.front() == '_') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    if (result.empty()) {
        result = "asset";
    }
    return result;
}

void appendUniqueTag(std::vector<std::string>& tags, std::string tag) {
    tag = trimAscii(std::move(tag));
    if (tag.empty()) {
        return;
    }
    const bool exists = std::find(tags.begin(), tags.end(), tag) != tags.end();
    if (!exists) {
        tags.push_back(std::move(tag));
    }
}

std::vector<std::byte> makeCheckerBmp(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b) {
    constexpr int kSize = 16;
    constexpr int kBytesPerPixel = 3;
    constexpr int kRowStride = ((kSize * kBytesPerPixel + 3) / 4) * 4;
    constexpr int kPixelDataSize = kRowStride * kSize;
    constexpr int kFileSize = 14 + 40 + kPixelDataSize;

    std::vector<std::byte> bytes(static_cast<std::size_t>(kFileSize), std::byte{0});
    auto writeU16 = [&](const std::size_t offset, const std::uint16_t value) {
        bytes[offset + 0] = static_cast<std::byte>(value & 0xff);
        bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
    };
    auto writeU32 = [&](const std::size_t offset, const std::uint32_t value) {
        bytes[offset + 0] = static_cast<std::byte>(value & 0xff);
        bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<std::byte>((value >> 24) & 0xff);
    };

    bytes[0] = std::byte{'B'};
    bytes[1] = std::byte{'M'};
    writeU32(2, kFileSize);
    writeU32(10, 14 + 40);
    writeU32(14, 40);
    writeU32(18, kSize);
    writeU32(22, kSize);
    writeU16(26, 1);
    writeU16(28, 24);
    writeU32(34, kPixelDataSize);

    const std::size_t pixelOffset = 14 + 40;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const bool checker = ((x / 4) + (y / 4)) % 2 == 0;
            const int tint = checker ? 20 : -12;
            const std::uint8_t rr = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(r) + tint, 0, 255));
            const std::uint8_t gg = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(g) + tint, 0, 255));
            const std::uint8_t bb = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(b) + tint, 0, 255));
            const std::size_t row = static_cast<std::size_t>(kSize - 1 - y) * kRowStride;
            const std::size_t pixel = row + static_cast<std::size_t>(x) * kBytesPerPixel;
            bytes[pixelOffset + pixel + 0] = static_cast<std::byte>(bb);
            bytes[pixelOffset + pixel + 1] = static_cast<std::byte>(gg);
            bytes[pixelOffset + pixel + 2] = static_cast<std::byte>(rr);
        }
    }

    return bytes;
}

std::vector<std::byte> makeBmpFromRgbBuffer(const int width,
                                            const int height,
                                            const std::vector<std::uint8_t>& pixels) {
    constexpr int kBytesPerPixel = 3;
    const int rowStride = ((width * kBytesPerPixel + 3) / 4) * 4;
    const int pixelDataSize = rowStride * height;
    const int fileSize = 14 + 40 + pixelDataSize;

    std::vector<std::byte> bytes(static_cast<std::size_t>(fileSize), std::byte{0});
    auto writeU16 = [&](const std::size_t offset, const std::uint16_t value) {
        bytes[offset + 0] = static_cast<std::byte>(value & 0xff);
        bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
    };
    auto writeU32 = [&](const std::size_t offset, const std::uint32_t value) {
        bytes[offset + 0] = static_cast<std::byte>(value & 0xff);
        bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<std::byte>((value >> 24) & 0xff);
    };

    writeU16(0, 0x4d42);
    writeU32(2, static_cast<std::uint32_t>(fileSize));
    writeU32(10, 54);
    writeU32(14, 40);
    writeU32(18, static_cast<std::uint32_t>(width));
    writeU32(22, static_cast<std::uint32_t>(height));
    writeU16(26, 1);
    writeU16(28, 24);
    writeU32(34, static_cast<std::uint32_t>(pixelDataSize));

    for (int y = 0; y < height; ++y) {
        const int srcY = height - 1 - y;
        const std::size_t srcOffset = static_cast<std::size_t>(srcY * width * kBytesPerPixel);
        const std::size_t dstOffset = static_cast<std::size_t>(54 + y * rowStride);
        for (int x = 0; x < width; ++x) {
            const std::size_t srcIndex = srcOffset + static_cast<std::size_t>(x * kBytesPerPixel);
            const std::size_t dstIndex = dstOffset + static_cast<std::size_t>(x * kBytesPerPixel);
            bytes[dstIndex + 0] = static_cast<std::byte>(pixels[srcIndex + 2]);
            bytes[dstIndex + 1] = static_cast<std::byte>(pixels[srcIndex + 1]);
            bytes[dstIndex + 2] = static_cast<std::byte>(pixels[srcIndex + 0]);
        }
    }
    return bytes;
}

util::Vec3 subtractVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

util::Vec3 multiplyVec3(const util::Vec3& value, const float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float dotVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

util::Vec3 crossVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float lengthVec3(const util::Vec3& value) {
    return std::sqrt(dotVec3(value, value));
}

util::Vec3 normalizeVec3(const util::Vec3& value) {
    const float length = lengthVec3(value);
    if (length <= 0.00001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return multiplyVec3(value, 1.0f / length);
}

float clamp01(const float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

std::filesystem::path generatedModelThumbnailPath(const std::filesystem::path& relativeModelPath) {
    return std::filesystem::path(kThumbnailRoot) / "models" /
        (sanitizeId(relativeModelPath.generic_string()) + ".bmp");
}

std::filesystem::path generateModelThumbnail(const std::filesystem::path& assetRoot,
                                             const std::filesystem::path& absoluteModelPath,
                                             const std::filesystem::path& relativeModelPath) {
    const std::filesystem::path outputRelativePath = generatedModelThumbnailPath(relativeModelPath);
    const std::filesystem::path outputAbsolutePath = assetRoot / outputRelativePath;
    if (util::FileSystem::exists(outputAbsolutePath)) {
        return outputRelativePath;
    }

    const renderer::vulkan::CpuMesh mesh = renderer::vulkan::loadMeshFromSource(absoluteModelPath);
    if (!mesh.valid || mesh.vertices.size() < 3) {
        return {};
    }

    constexpr int kSize = 192;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize * kSize * 3), 0);
    std::vector<float> depth(static_cast<std::size_t>(kSize * kSize), std::numeric_limits<float>::infinity());
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float blend = clamp01(static_cast<float>(y) / static_cast<float>(kSize - 1));
            const std::uint8_t r = static_cast<std::uint8_t>(std::lround(18.0f + blend * 18.0f));
            const std::uint8_t g = static_cast<std::uint8_t>(std::lround(22.0f + blend * 20.0f));
            const std::uint8_t b = static_cast<std::uint8_t>(std::lround(28.0f + blend * 24.0f));
            const std::size_t index = static_cast<std::size_t>((y * kSize + x) * 3);
            pixels[index + 0] = r;
            pixels[index + 1] = g;
            pixels[index + 2] = b;
        }
    }

    const float yaw = 0.72f;
    const float pitch = -0.48f;
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    const float invRadius = 1.0f / std::max(0.001f, mesh.radius);
    const float cameraDistance = 3.8f;
    const float projectionScale = static_cast<float>(kSize) * 0.42f;
    const float centerX = static_cast<float>(kSize) * 0.5f;
    const float centerY = static_cast<float>(kSize) * 0.56f;
    const util::Vec3 lightDirection = normalizeVec3({-0.45f, 0.72f, -0.54f});

    struct ProjectedVertex {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    auto transformVertex = [&](const renderer::vulkan::MeshVertex& source) {
        util::Vec3 p = {
            (source.px - mesh.center.x) * invRadius,
            (source.py - mesh.center.y) * invRadius,
            (source.pz - mesh.center.z) * invRadius,
        };
        const float yawX = p.x * cosYaw - p.z * sinYaw;
        const float yawZ = p.x * sinYaw + p.z * cosYaw;
        const float pitchY = p.y * cosPitch - yawZ * sinPitch;
        const float pitchZ = p.y * sinPitch + yawZ * cosPitch + cameraDistance;
        return util::Vec3{yawX, pitchY, pitchZ};
    };

    auto projectVertex = [&](const util::Vec3& value) {
        return ProjectedVertex{
            centerX + (value.x / value.z) * projectionScale,
            centerY - (value.y / value.z) * projectionScale,
            value.z,
        };
    };

    auto edge = [](const ProjectedVertex& a, const ProjectedVertex& b, const float px, const float py) {
        return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
    };

    for (std::size_t index = 0; index + 2 < mesh.vertices.size(); index += 3) {
        const auto& va = mesh.vertices[index + 0];
        const auto& vb = mesh.vertices[index + 1];
        const auto& vc = mesh.vertices[index + 2];
        const util::Vec3 ta = transformVertex(va);
        const util::Vec3 tb = transformVertex(vb);
        const util::Vec3 tc = transformVertex(vc);
        if (ta.z <= 0.01f || tb.z <= 0.01f || tc.z <= 0.01f) {
            continue;
        }

        const util::Vec3 normal = crossVec3(subtractVec3(tb, ta), subtractVec3(tc, ta));
        const util::Vec3 normalizedNormal = normalizeVec3(normal);
        if (normalizedNormal.z >= -0.02f) {
            continue;
        }

        const ProjectedVertex pa = projectVertex(ta);
        const ProjectedVertex pb = projectVertex(tb);
        const ProjectedVertex pc = projectVertex(tc);
        const float minX = std::floor(std::min({pa.x, pb.x, pc.x}));
        const float maxX = std::ceil(std::max({pa.x, pb.x, pc.x}));
        const float minY = std::floor(std::min({pa.y, pb.y, pc.y}));
        const float maxY = std::ceil(std::max({pa.y, pb.y, pc.y}));
        const int x0 = std::clamp(static_cast<int>(minX), 0, kSize - 1);
        const int x1 = std::clamp(static_cast<int>(maxX), 0, kSize - 1);
        const int y0 = std::clamp(static_cast<int>(minY), 0, kSize - 1);
        const int y1 = std::clamp(static_cast<int>(maxY), 0, kSize - 1);
        const float area = edge(pa, pb, pc.x, pc.y);
        if (std::abs(area) <= 0.00001f) {
            continue;
        }

        const util::Vec3 baseColor = {
            (va.r + vb.r + vc.r) / 3.0f,
            (va.g + vb.g + vc.g) / 3.0f,
            (va.b + vb.b + vc.b) / 3.0f,
        };
        const float light = 0.28f + 0.72f * clamp01(dotVec3(normalizedNormal, multiplyVec3(lightDirection, -1.0f)));
        const std::uint8_t r = static_cast<std::uint8_t>(std::lround(clamp01(baseColor.x * light) * 255.0f));
        const std::uint8_t g = static_cast<std::uint8_t>(std::lround(clamp01(baseColor.y * light) * 255.0f));
        const std::uint8_t b = static_cast<std::uint8_t>(std::lround(clamp01(baseColor.z * light) * 255.0f));

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const float w0 = edge(pb, pc, px, py) / area;
                const float w1 = edge(pc, pa, px, py) / area;
                const float w2 = edge(pa, pb, px, py) / area;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }
                const float z = pa.z * w0 + pb.z * w1 + pc.z * w2;
                const std::size_t pixelIndex = static_cast<std::size_t>(y * kSize + x);
                if (z >= depth[pixelIndex]) {
                    continue;
                }
                depth[pixelIndex] = z;
                const std::size_t colorIndex = pixelIndex * 3;
                pixels[colorIndex + 0] = r;
                pixels[colorIndex + 1] = g;
                pixels[colorIndex + 2] = b;
            }
        }
    }

    if (!util::FileSystem::writeBinary(outputAbsolutePath, makeBmpFromRgbBuffer(kSize, kSize, pixels))) {
        return {};
    }
    return outputRelativePath;
}

std::string escapeJson(const std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const unsigned char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20) {
                    char buffer[7];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(character));
                    escaped += buffer;
                } else {
                    escaped.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    return escaped;
}

std::filesystem::path relativeToRoot(const std::filesystem::path& assetRoot,
                                     const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path relative = std::filesystem::relative(path, assetRoot, error);
    if (error) {
        return path.filename();
    }
    return relative;
}

bool hasSupportedModelExtension(const std::string_view extension) {
    return extension == ".obj" || extension == ".gltf" || extension == ".glb";
}

bool hasManifestModelExtension(const std::string_view extension) {
    return hasSupportedModelExtension(extension) || extension == ".fbx" || extension == ".dae";
}

bool hasManifestTextureExtension(const std::string_view extension) {
    return extension == ".bmp" ||
           extension == ".png" ||
           extension == ".jpg" ||
           extension == ".jpeg" ||
           extension == ".ppm" ||
           extension == ".tga";
}

std::string detectModelCategory(const std::filesystem::path& relativePath) {
    const std::string text = lowerAscii(relativePath.generic_string());
    if (text.find("/characters/") != std::string::npos ||
        text.find("superhero") != std::string::npos ||
        text.find("operator") != std::string::npos) {
        return "角色";
    }
    if (text.find("hair") != std::string::npos ||
        text.find("eyebrows") != std::string::npos ||
        text.find("beard") != std::string::npos) {
        return "角色附件";
    }
    if (text.find("/weapons/") != std::string::npos ||
        text.find("rifle") != std::string::npos ||
        text.find("shotgun") != std::string::npos ||
        text.find("submachine") != std::string::npos ||
        text.find("pistol") != std::string::npos ||
        text.find("revolver") != std::string::npos ||
        text.find("bayonet") != std::string::npos) {
        return "武器";
    }
    if (text.find("crate") != std::string::npos ||
        text.find("barrel") != std::string::npos ||
        text.find("box") != std::string::npos ||
        text.find("/props/") != std::string::npos) {
        return "道具";
    }
    if (text.rfind("generated/models/", 0) == 0) {
        return "生成模型";
    }
    return "场景";
}

std::string detectTextureCategory(const std::filesystem::path& relativePath) {
    const std::string text = lowerAscii(relativePath.filename().string());
    if (text.find("preview") != std::string::npos) {
        return "预览图";
    }
    if (text.find("normal") != std::string::npos || text.find("_nor") != std::string::npos) {
        return "法线";
    }
    if (text.find("rough") != std::string::npos) {
        return "粗糙度";
    }
    if (text.find("_arm") != std::string::npos ||
        text.find("_ao") != std::string::npos ||
        text.find("occlusion") != std::string::npos ||
        text.find("metal") != std::string::npos) {
        return "材质遮罩";
    }
    if (text.find("basecolor") != std::string::npos ||
        text.find("albedo") != std::string::npos ||
        text.find("_diff") != std::string::npos ||
        text.find("diffuse") != std::string::npos) {
        return "基础色";
    }
    return "贴图";
}

std::string detectMaterialCategory(const std::filesystem::path& materialRelativePath,
                                   const std::filesystem::path& albedoRelativePath) {
    if (!albedoRelativePath.empty()) {
        const std::string textureCategory = detectTextureCategory(albedoRelativePath);
        if (textureCategory == "基础色" || textureCategory == "预览图" || textureCategory == "贴图") {
            return "表面材质";
        }
    }
    const std::string text = lowerAscii(materialRelativePath.generic_string());
    if (text.find("bomb_site") != std::string::npos) {
        return "标记材质";
    }
    if (text.find("default") != std::string::npos) {
        return "默认材质";
    }
    return "表面材质";
}

std::string sourceLabelForPath(const std::filesystem::path& relativePath) {
    const std::string generic = relativePath.generic_string();
    if (generic.rfind("source/polyhaven/", 0) == 0) {
        return "PolyHaven";
    }
    if (generic.rfind("source/itchio/", 0) == 0) {
        return "itch.io";
    }
    if (generic.rfind("source/weapons/", 0) == 0) {
        return "武器资源";
    }
    if (generic.rfind("source/characters/", 0) == 0) {
        return "角色资源";
    }
    if (generic.rfind("generated/", 0) == 0) {
        return "生成资源";
    }
    return "资源目录";
}

std::string formatLabelForPath(const std::filesystem::path& path) {
    std::string extension = lowerAscii(path.extension().string());
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return extension.empty() ? "FILE" : extension;
}

std::filesystem::path ensureFallbackThumbnail(const std::filesystem::path& assetRoot,
                                              const std::string_view key,
                                              const std::array<std::uint8_t, 3>& color) {
    const std::filesystem::path relativePath = std::filesystem::path(kThumbnailRoot) / (std::string(key) + ".bmp");
    const std::filesystem::path absolutePath = assetRoot / relativePath;
    if (util::FileSystem::exists(absolutePath)) {
        return relativePath;
    }
    if (!util::FileSystem::writeBinary(absolutePath, makeCheckerBmp(color[0], color[1], color[2]))) {
        return {};
    }
    return relativePath;
}

std::filesystem::path fallbackThumbnailPathForCategory(const std::filesystem::path& assetRoot,
                                                       const std::string& categoryLabel) {
    if (categoryLabel == "角色") {
        return ensureFallbackThumbnail(assetRoot, "category_character", {78, 134, 178});
    }
    if (categoryLabel == "角色附件") {
        return ensureFallbackThumbnail(assetRoot, "category_character_accessory", {110, 94, 176});
    }
    if (categoryLabel == "武器") {
        return ensureFallbackThumbnail(assetRoot, "category_weapon", {162, 96, 68});
    }
    if (categoryLabel == "道具") {
        return ensureFallbackThumbnail(assetRoot, "category_prop", {124, 110, 72});
    }
    if (categoryLabel == "基础色") {
        return ensureFallbackThumbnail(assetRoot, "category_albedo", {140, 140, 140});
    }
    if (categoryLabel == "法线") {
        return ensureFallbackThumbnail(assetRoot, "category_normal", {102, 132, 208});
    }
    if (categoryLabel == "粗糙度") {
        return ensureFallbackThumbnail(assetRoot, "category_roughness", {122, 122, 122});
    }
    if (categoryLabel == "材质遮罩") {
        return ensureFallbackThumbnail(assetRoot, "category_mask", {86, 146, 116});
    }
    if (categoryLabel == "标记材质") {
        return ensureFallbackThumbnail(assetRoot, "category_marker_material", {170, 82, 72});
    }
    if (categoryLabel == "默认材质") {
        return ensureFallbackThumbnail(assetRoot, "category_default_material", {98, 104, 116});
    }
    if (categoryLabel == "预览图") {
        return ensureFallbackThumbnail(assetRoot, "category_preview", {164, 130, 86});
    }
    if (categoryLabel == "生成模型") {
        return ensureFallbackThumbnail(assetRoot, "category_generated_model", {96, 124, 138});
    }
    if (categoryLabel == "表面材质") {
        return ensureFallbackThumbnail(assetRoot, "category_material", {120, 128, 96});
    }
    if (categoryLabel == "场景") {
        return ensureFallbackThumbnail(assetRoot, "category_scene", {98, 132, 92});
    }
    return ensureFallbackThumbnail(assetRoot, "category_generic", {104, 112, 124});
}

bool isImagePathCandidate(const std::filesystem::path& path) {
    return hasManifestTextureExtension(lowerAscii(path.extension().string()));
}

int previewCandidateScore(const std::filesystem::path& path) {
    const std::string lowerName = lowerAscii(path.filename().string());
    int score = 0;

    if (lowerName.find("preview") != std::string::npos) {
        score += 200;
    }
    if (lowerName.find("basecolor") != std::string::npos || lowerName.find("albedo") != std::string::npos) {
        score += 140;
    }
    if (lowerName.find("_diff") != std::string::npos || lowerName.find("diffuse") != std::string::npos) {
        score += 120;
    }
    if (lowerName.find("color") != std::string::npos) {
        score += 80;
    }
    if (lowerName.find("normal") != std::string::npos || lowerName.find("_nor") != std::string::npos) {
        score -= 120;
    }
    if (lowerName.find("rough") != std::string::npos ||
        lowerName.find("metal") != std::string::npos ||
        lowerName.find("_arm") != std::string::npos ||
        lowerName.find("_ao") != std::string::npos ||
        lowerName.find("occlusion") != std::string::npos ||
        lowerName.find("mask") != std::string::npos ||
        lowerName.find("spec") != std::string::npos ||
        lowerName.find("opacity") != std::string::npos ||
        lowerName.find("emiss") != std::string::npos) {
        score -= 80;
    }

    return score;
}

std::filesystem::path searchPreviewTexture(const std::filesystem::path& absoluteDirectory) {
    if (!util::FileSystem::exists(absoluteDirectory)) {
        return {};
    }
    std::error_code error;
    std::filesystem::directory_iterator it(absoluteDirectory, error);
    std::filesystem::directory_iterator end;
    std::filesystem::path bestCandidate{};
    int bestScore = std::numeric_limits<int>::min();
    while (!error && it != end) {
        if (it->is_regular_file(error) && !error && isImagePathCandidate(it->path())) {
            const int score = previewCandidateScore(it->path());
            if (bestCandidate.empty() ||
                score > bestScore ||
                (score == bestScore && it->path().filename().generic_string() < bestCandidate.filename().generic_string())) {
                bestCandidate = it->path();
                bestScore = score;
            }
        }
        it.increment(error);
    }
    return bestCandidate;
}

std::filesystem::path resolveModelThumbnailPath(const std::filesystem::path& assetRoot,
                                                const std::filesystem::path& absoluteModelPath,
                                                const std::string& categoryLabel) {
    const std::filesystem::path directPreview = searchPreviewTexture(absoluteModelPath.parent_path());
    if (!directPreview.empty()) {
        return relativeToRoot(assetRoot, directPreview);
    }

    const std::filesystem::path parent = absoluteModelPath.parent_path();
    const std::array candidateDirs{
        parent / "textures",
        parent / "Textures",
        parent.parent_path() / "textures",
        parent.parent_path() / "Textures",
        parent.parent_path(),
    };
    for (const auto& directory : candidateDirs) {
        const std::filesystem::path candidate = searchPreviewTexture(directory);
        if (!candidate.empty()) {
            return relativeToRoot(assetRoot, candidate);
        }
    }

    const std::array previewNames{
        "Preview.png",
        "Preview.jpg",
        "preview.png",
        "preview.jpg",
    };
    std::filesystem::path ancestor = parent;
    for (int depth = 0; depth < 3 && !ancestor.empty(); ++depth) {
        for (const char* previewName : previewNames) {
            const std::filesystem::path candidate = ancestor / previewName;
            if (util::FileSystem::exists(candidate)) {
                return relativeToRoot(assetRoot, candidate);
            }
        }
        ancestor = ancestor.parent_path();
    }

    const std::string extension = lowerAscii(absoluteModelPath.extension().string());
    if (hasSupportedModelExtension(extension)) {
        const std::filesystem::path generatedThumbnail = generateModelThumbnail(
            assetRoot,
            absoluteModelPath,
            relativeToRoot(assetRoot, absoluteModelPath));
        if (!generatedThumbnail.empty()) {
            return generatedThumbnail;
        }
    }

    return fallbackThumbnailPathForCategory(assetRoot, categoryLabel);
}

std::vector<std::string> buildModelTags(const std::filesystem::path& relativePath,
                                        const std::string& sourceLabel,
                                        const std::string& categoryLabel,
                                        const std::string& formatLabel,
                                        const bool runtimeSupported) {
    std::vector<std::string> tags;
    appendUniqueTag(tags, sourceLabel);
    appendUniqueTag(tags, categoryLabel);
    appendUniqueTag(tags, formatLabel);
    appendUniqueTag(tags, runtimeSupported ? "运行时可用" : "仅归档");

    const std::string text = lowerAscii(relativePath.generic_string());
    if (text.find("crate") != std::string::npos || text.find("box") != std::string::npos) {
        appendUniqueTag(tags, "箱体");
    }
    if (text.find("barrel") != std::string::npos) {
        appendUniqueTag(tags, "油桶");
    }
    if (text.find("metro") != std::string::npos) {
        appendUniqueTag(tags, "地铁");
    }
    if (text.find("rifle") != std::string::npos || text.find("gun") != std::string::npos) {
        appendUniqueTag(tags, "枪械");
    }
    if (text.find("sniper") != std::string::npos) {
        appendUniqueTag(tags, "狙击");
    }
    if (text.find("shotgun") != std::string::npos) {
        appendUniqueTag(tags, "霰弹");
    }
    if (text.find("hair") != std::string::npos || text.find("eyebrows") != std::string::npos || text.find("beard") != std::string::npos) {
        appendUniqueTag(tags, "头发");
    }
    return tags;
}

std::vector<std::string> buildTextureTags(const std::filesystem::path& relativePath,
                                          const std::string& sourceLabel,
                                          const std::string& categoryLabel,
                                          const std::string& formatLabel) {
    std::vector<std::string> tags;
    appendUniqueTag(tags, sourceLabel);
    appendUniqueTag(tags, categoryLabel);
    appendUniqueTag(tags, formatLabel);
    appendUniqueTag(tags, "运行时可用");

    const std::string text = lowerAscii(relativePath.generic_string());
    if (text.find("wood") != std::string::npos) {
        appendUniqueTag(tags, "木质");
    }
    if (text.find("metal") != std::string::npos) {
        appendUniqueTag(tags, "金属");
    }
    if (text.find("concrete") != std::string::npos) {
        appendUniqueTag(tags, "混凝土");
    }
    if (text.find("hair") != std::string::npos) {
        appendUniqueTag(tags, "头发");
    }
    return tags;
}

std::vector<std::string> buildMaterialTags(const std::filesystem::path& materialRelativePath,
                                           const std::filesystem::path& albedoRelativePath,
                                           const std::string& categoryLabel,
                                           const bool generatedFromTexture) {
    std::vector<std::string> tags;
    appendUniqueTag(tags, categoryLabel);
    appendUniqueTag(tags, generatedFromTexture ? "自动生成" : "材质文件");
    appendUniqueTag(tags, "运行时可用");
    if (!albedoRelativePath.empty()) {
        appendUniqueTag(tags, detectTextureCategory(albedoRelativePath));
    }
    const std::string text = lowerAscii(materialRelativePath.generic_string());
    if (text.find("polyhaven") != std::string::npos) {
        appendUniqueTag(tags, "PolyHaven");
    }
    if (text.find("classic64") != std::string::npos) {
        appendUniqueTag(tags, "itch.io");
    }
    return tags;
}

std::string displayLabelForPath(const std::filesystem::path& relativePath) {
    const std::string sourceLabel = sourceLabelForPath(relativePath);
    const std::string stem = prettifyToken(relativePath.stem().string());
    const std::string parent = prettifyToken(relativePath.parent_path().filename().string());
    const std::string formatLabel = formatLabelForPath(relativePath);

    std::ostringstream out;
    out << sourceLabel << " / ";
    if (!parent.empty() && lowerAscii(parent) != lowerAscii(stem)) {
        out << parent << " / ";
    }
    out << (stem.empty() ? relativePath.filename().string() : stem)
        << " [" << formatLabel << "]";
    return out.str();
}

AssetManifestEntry makeAssetEntry(const std::filesystem::path& assetRoot,
                                  const std::filesystem::path& absolutePath) {
    const std::filesystem::path relativePath = relativeToRoot(assetRoot, absolutePath);
    const std::string extension = lowerAscii(relativePath.extension().string());
    const bool isModel = hasManifestModelExtension(extension);
    const std::string sourceLabel = sourceLabelForPath(relativePath);
    const std::string categoryLabel = isModel ? detectModelCategory(relativePath) : detectTextureCategory(relativePath);
    const std::string formatLabel = formatLabelForPath(relativePath);
    const bool runtimeSupported = hasSupportedModelExtension(extension) || hasManifestTextureExtension(extension);
    return AssetManifestEntry{
        .id = sanitizeId(relativePath.generic_string()),
        .displayName = displayLabelForPath(relativePath),
        .path = relativePath,
        .sourceLabel = sourceLabel,
        .categoryLabel = categoryLabel,
        .formatLabel = formatLabel,
        .tags = isModel
            ? buildModelTags(relativePath, sourceLabel, categoryLabel, formatLabel, runtimeSupported)
            : buildTextureTags(relativePath, sourceLabel, categoryLabel, formatLabel),
        .thumbnailPath = isModel
            ? resolveModelThumbnailPath(assetRoot, absolutePath, categoryLabel)
            : relativePath,
        .runtimeSupported = runtimeSupported,
    };
}

bool shouldAutoGenerateMaterialFromTexture(const std::filesystem::path& relativePath) {
    const std::string lowerName = lowerAscii(relativePath.filename().string());
    static constexpr std::string_view kExcludedTokens[] = {
        "normal",
        "_nor",
        "rough",
        "roughness",
        "_arm",
        "_ao",
        "ambientocclusion",
        "occlusion",
        "_disp",
        "height",
        "emiss",
        "spec",
        "opacity",
        "mask",
    };
    for (const std::string_view token : kExcludedTokens) {
        if (lowerName.find(token) != std::string::npos) {
            return false;
        }
    }
    return true;
}

std::string parseMaterialKey(const std::filesystem::path& absolutePath, const std::string_view key) {
    const std::string materialText = util::FileSystem::readText(absolutePath);
    std::istringstream stream(materialText);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }
        const std::string currentKey = trimAscii(line.substr(0, equalsPos));
        if (lowerAscii(currentKey) != lowerAscii(std::string(key))) {
            continue;
        }
        return trimAscii(line.substr(equalsPos + 1));
    }
    return {};
}

MaterialAssetEntry makeMaterialEntry(const std::filesystem::path& materialRelativePath,
                                     const std::filesystem::path& albedoRelativePath,
                                     const bool generatedFromTexture,
                                     const std::string& displayPrefix = {},
                                     const std::string& categoryLabelOverride = {},
                                     const std::filesystem::path& thumbnailPathOverride = {},
                                     const std::filesystem::path& fallbackThumbnailPath = {}) {
    const std::string labelBase = displayLabelForPath(albedoRelativePath.empty() ? materialRelativePath : albedoRelativePath);
    const std::string categoryLabel = categoryLabelOverride.empty()
        ? detectMaterialCategory(materialRelativePath, albedoRelativePath)
        : categoryLabelOverride;
    return MaterialAssetEntry{
        .id = sanitizeId(materialRelativePath.generic_string()),
        .displayName = displayPrefix.empty() ? labelBase : (displayPrefix + " / " + labelBase),
        .materialPath = materialRelativePath,
        .albedoPath = albedoRelativePath,
        .categoryLabel = categoryLabel,
        .tags = buildMaterialTags(materialRelativePath, albedoRelativePath, categoryLabel, generatedFromTexture),
        .thumbnailPath = thumbnailPathOverride.empty()
            ? (!albedoRelativePath.empty() ? albedoRelativePath : fallbackThumbnailPath)
            : thumbnailPathOverride,
        .generatedFromTexture = generatedFromTexture,
    };
}

template <typename Predicate>
void scanDirectory(const std::filesystem::path& assetRoot,
                   const std::filesystem::path& scanRoot,
                   Predicate&& predicate,
                   std::vector<AssetManifestEntry>& outEntries) {
    if (!util::FileSystem::exists(scanRoot)) {
        return;
    }

    std::error_code error;
    std::filesystem::recursive_directory_iterator it(scanRoot, error);
    std::filesystem::recursive_directory_iterator end;
    while (!error && it != end) {
        const auto& entry = *it;
        if (entry.is_regular_file(error) && !error && predicate(entry.path())) {
            outEntries.push_back(makeAssetEntry(assetRoot, entry.path()));
        }
        it.increment(error);
    }
}

void sortAssetEntries(std::vector<AssetManifestEntry>& entries) {
    std::sort(entries.begin(), entries.end(), [](const AssetManifestEntry& lhs, const AssetManifestEntry& rhs) {
        if (lhs.categoryLabel != rhs.categoryLabel) {
            return lhs.categoryLabel < rhs.categoryLabel;
        }
        if (lhs.sourceLabel != rhs.sourceLabel) {
            return lhs.sourceLabel < rhs.sourceLabel;
        }
        if (lhs.displayName != rhs.displayName) {
            return lhs.displayName < rhs.displayName;
        }
        return lhs.path.generic_string() < rhs.path.generic_string();
    });
}

void sortMaterialEntries(std::vector<MaterialAssetEntry>& entries) {
    std::sort(entries.begin(), entries.end(), [](const MaterialAssetEntry& lhs, const MaterialAssetEntry& rhs) {
        if (lhs.categoryLabel != rhs.categoryLabel) {
            return lhs.categoryLabel < rhs.categoryLabel;
        }
        if (lhs.displayName != rhs.displayName) {
            return lhs.displayName < rhs.displayName;
        }
        return lhs.materialPath.generic_string() < rhs.materialPath.generic_string();
    });
}

}  // namespace

AssetManifest buildAssetManifest(const std::filesystem::path& assetRoot) {
    AssetManifest manifest{};
    manifest.manifestPath = kManifestFilePath;

    scanDirectory(assetRoot, assetRoot / "generated" / "models",
        [](const std::filesystem::path& path) {
            return hasManifestModelExtension(lowerAscii(path.extension().string()));
        },
        manifest.models);
    scanDirectory(assetRoot, assetRoot / "source",
        [](const std::filesystem::path& path) {
            return hasManifestModelExtension(lowerAscii(path.extension().string()));
        },
        manifest.models);
    sortAssetEntries(manifest.models);

    scanDirectory(assetRoot, assetRoot / "generated" / "textures",
        [](const std::filesystem::path& path) {
            return hasManifestTextureExtension(lowerAscii(path.extension().string()));
        },
        manifest.textures);
    scanDirectory(assetRoot, assetRoot / "source",
        [](const std::filesystem::path& path) {
            return hasManifestTextureExtension(lowerAscii(path.extension().string()));
        },
        manifest.textures);
    sortAssetEntries(manifest.textures);

    const std::filesystem::path materialRoot = assetRoot / "generated" / "materials";
    std::set<std::string> knownAlbedoPaths;
    if (util::FileSystem::exists(materialRoot)) {
        std::error_code error;
        std::filesystem::recursive_directory_iterator it(materialRoot, error);
        std::filesystem::recursive_directory_iterator end;
        while (!error && it != end) {
            const auto& entry = *it;
            if (entry.is_regular_file(error) && !error && lowerAscii(entry.path().extension().string()) == ".mat") {
                const std::filesystem::path materialRelativePath = relativeToRoot(assetRoot, entry.path());
                const std::string albedoValue = parseMaterialKey(entry.path(), "albedo");
                const std::filesystem::path albedoRelativePath = albedoValue.empty()
                    ? std::filesystem::path{}
                    : std::filesystem::path(albedoValue);
                manifest.materials.push_back(makeMaterialEntry(
                    materialRelativePath,
                    albedoRelativePath,
                    false,
                    {},
                    {},
                    {},
                    fallbackThumbnailPathForCategory(assetRoot, detectMaterialCategory(materialRelativePath, albedoRelativePath))));
                if (!albedoRelativePath.empty()) {
                    knownAlbedoPaths.insert(albedoRelativePath.generic_string());
                }
            }
            it.increment(error);
        }
    }

    const std::filesystem::path autoMaterialDir = assetRoot / std::filesystem::path(kAutoMaterialRoot);
    util::FileSystem::ensureDirectory(autoMaterialDir);
    for (const AssetManifestEntry& texture : manifest.textures) {
        if (!texture.runtimeSupported || !shouldAutoGenerateMaterialFromTexture(texture.path)) {
            continue;
        }
        const std::string textureKey = texture.path.generic_string();
        if (knownAlbedoPaths.contains(textureKey)) {
            continue;
        }

        const std::filesystem::path materialRelativePath =
            std::filesystem::path(kAutoMaterialRoot) / (texture.id + ".mat");
        std::ostringstream materialText;
        materialText << "albedo=" << texture.path.generic_string() << '\n'
            << "roughness=0.72\n"
            << "metallic=0.0\n";
        util::FileSystem::writeText(assetRoot / materialRelativePath, materialText.str());
        manifest.materials.push_back(makeMaterialEntry(
            materialRelativePath,
            texture.path,
            true,
            "贴图材质",
            {},
            {},
            fallbackThumbnailPathForCategory(assetRoot, detectMaterialCategory(materialRelativePath, texture.path))));
        knownAlbedoPaths.insert(textureKey);
    }
    sortMaterialEntries(manifest.materials);

    for (const AssetManifestEntry& model : manifest.models) {
        if (model.runtimeSupported) {
            manifest.editorModels.push_back(model);
        }
    }
    sortAssetEntries(manifest.editorModels);

    manifest.editorMaterials.push_back(MaterialAssetEntry{
        .id = "model_default",
        .displayName = "跟随模型原色",
        .materialPath = {},
        .albedoPath = {},
        .categoryLabel = "模型默认",
        .tags = {"模型默认", "运行时可用"},
        .thumbnailPath = fallbackThumbnailPathForCategory(assetRoot, "默认材质"),
        .generatedFromTexture = false,
    });
    manifest.editorMaterials.insert(
        manifest.editorMaterials.end(),
        manifest.materials.begin(),
        manifest.materials.end());

    return manifest;
}

bool writeAssetManifest(const std::filesystem::path& assetRoot, const AssetManifest& manifest) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"manifest\": \"" << escapeJson(manifest.manifestPath.generic_string()) << "\",\n";

    const auto writeAssetList = [&](const char* label,
                                    const std::vector<AssetManifestEntry>& entries,
                                    const bool trailingComma) {
        out << "  \"" << label << "\": [\n";
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const AssetManifestEntry& entry = entries[index];
            out << "    {\n";
            out << "      \"id\": \"" << escapeJson(entry.id) << "\",\n";
            out << "      \"displayName\": \"" << escapeJson(entry.displayName) << "\",\n";
            out << "      \"path\": \"" << escapeJson(entry.path.generic_string()) << "\",\n";
            out << "      \"source\": \"" << escapeJson(entry.sourceLabel) << "\",\n";
            out << "      \"category\": \"" << escapeJson(entry.categoryLabel) << "\",\n";
            out << "      \"format\": \"" << escapeJson(entry.formatLabel) << "\",\n";
            out << "      \"thumbnailPath\": \"" << escapeJson(entry.thumbnailPath.generic_string()) << "\",\n";
            out << "      \"tags\": [";
            for (std::size_t tagIndex = 0; tagIndex < entry.tags.size(); ++tagIndex) {
                out << '"' << escapeJson(entry.tags[tagIndex]) << '"';
                if (tagIndex + 1 < entry.tags.size()) {
                    out << ", ";
                }
            }
            out << "],\n";
            out << "      \"runtimeSupported\": " << (entry.runtimeSupported ? "true" : "false") << '\n';
            out << "    }" << (index + 1 < entries.size() ? "," : "") << '\n';
        }
        out << "  ]" << (trailingComma ? "," : "") << '\n';
    };

    const auto writeMaterialList = [&](const char* label,
                                       const std::vector<MaterialAssetEntry>& entries,
                                       const bool trailingComma) {
        out << "  \"" << label << "\": [\n";
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const MaterialAssetEntry& entry = entries[index];
            out << "    {\n";
            out << "      \"id\": \"" << escapeJson(entry.id) << "\",\n";
            out << "      \"displayName\": \"" << escapeJson(entry.displayName) << "\",\n";
            out << "      \"materialPath\": \"" << escapeJson(entry.materialPath.generic_string()) << "\",\n";
            out << "      \"albedoPath\": \"" << escapeJson(entry.albedoPath.generic_string()) << "\",\n";
            out << "      \"category\": \"" << escapeJson(entry.categoryLabel) << "\",\n";
            out << "      \"thumbnailPath\": \"" << escapeJson(entry.thumbnailPath.generic_string()) << "\",\n";
            out << "      \"tags\": [";
            for (std::size_t tagIndex = 0; tagIndex < entry.tags.size(); ++tagIndex) {
                out << '"' << escapeJson(entry.tags[tagIndex]) << '"';
                if (tagIndex + 1 < entry.tags.size()) {
                    out << ", ";
                }
            }
            out << "],\n";
            out << "      \"generatedFromTexture\": " << (entry.generatedFromTexture ? "true" : "false") << '\n';
            out << "    }" << (index + 1 < entries.size() ? "," : "") << '\n';
        }
        out << "  ]" << (trailingComma ? "," : "") << '\n';
    };

    writeAssetList("models", manifest.models, true);
    writeAssetList("textures", manifest.textures, true);
    writeMaterialList("materials", manifest.materials, true);
    writeAssetList("editorModels", manifest.editorModels, true);
    writeMaterialList("editorMaterials", manifest.editorMaterials, false);
    out << "}\n";

    return util::FileSystem::writeText(assetRoot / manifest.manifestPath, out.str());
}

}  // namespace mycsg::content
