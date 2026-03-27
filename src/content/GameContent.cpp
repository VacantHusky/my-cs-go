#include "content/GameContent.h"

#include "util/FileSystem.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <vector>

namespace mycsg::content {

namespace {

std::vector<std::byte> makeBmp(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    constexpr int kSize = 8;
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
            const bool checker = ((x + y) % 2) == 0;
            const int tint = checker ? 18 : -18;
            const std::uint8_t rr = static_cast<std::uint8_t>(std::clamp<int>(r + tint, 0, 255));
            const std::uint8_t gg = static_cast<std::uint8_t>(std::clamp<int>(g + tint, 0, 255));
            const std::uint8_t bb = static_cast<std::uint8_t>(std::clamp<int>(b + tint, 0, 255));
            const std::size_t row = static_cast<std::size_t>(kSize - 1 - y) * kRowStride;
            const std::size_t pixel = row + static_cast<std::size_t>(x) * kBytesPerPixel;
            bytes[pixelOffset + pixel + 0] = static_cast<std::byte>(bb);
            bytes[pixelOffset + pixel + 1] = static_cast<std::byte>(gg);
            bytes[pixelOffset + pixel + 2] = static_cast<std::byte>(rr);
        }
    }

    return bytes;
}

std::string makeCubeObj() {
    return
        "o generated_cube\n"
        "v -0.5 0.0 -0.5\n"
        "v 0.5 0.0 -0.5\n"
        "v 0.5 1.0 -0.5\n"
        "v -0.5 1.0 -0.5\n"
        "v -0.5 0.0 0.5\n"
        "v 0.5 0.0 0.5\n"
        "v 0.5 1.0 0.5\n"
        "v -0.5 1.0 0.5\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "f 1/1 2/2 3/3 4/4\n"
        "f 5/1 8/4 7/3 6/2\n"
        "f 1/1 5/2 6/3 2/4\n"
        "f 2/1 6/2 7/3 3/4\n"
        "f 3/1 7/2 8/3 4/4\n"
        "f 5/1 1/2 4/3 8/4\n";
}

struct HumanoidBox {
    const char* material = "";
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
};

void appendHumanoidBox(std::ostringstream& out, int& nextVertexIndex, const HumanoidBox& box) {
    out << "usemtl " << box.material << '\n';
    out << "v " << box.minX << ' ' << box.minY << ' ' << box.minZ << '\n';
    out << "v " << box.maxX << ' ' << box.minY << ' ' << box.minZ << '\n';
    out << "v " << box.maxX << ' ' << box.maxY << ' ' << box.minZ << '\n';
    out << "v " << box.minX << ' ' << box.maxY << ' ' << box.minZ << '\n';
    out << "v " << box.minX << ' ' << box.minY << ' ' << box.maxZ << '\n';
    out << "v " << box.maxX << ' ' << box.minY << ' ' << box.maxZ << '\n';
    out << "v " << box.maxX << ' ' << box.maxY << ' ' << box.maxZ << '\n';
    out << "v " << box.minX << ' ' << box.maxY << ' ' << box.maxZ << '\n';

    const int base = nextVertexIndex;
    out << "f " << base + 0 << ' ' << base + 3 << ' ' << base + 2 << ' ' << base + 1 << '\n';
    out << "f " << base + 4 << ' ' << base + 5 << ' ' << base + 6 << ' ' << base + 7 << '\n';
    out << "f " << base + 0 << ' ' << base + 1 << ' ' << base + 5 << ' ' << base + 4 << '\n';
    out << "f " << base + 1 << ' ' << base + 2 << ' ' << base + 6 << ' ' << base + 5 << '\n';
    out << "f " << base + 3 << ' ' << base + 7 << ' ' << base + 6 << ' ' << base + 2 << '\n';
    out << "f " << base + 0 << ' ' << base + 4 << ' ' << base + 7 << ' ' << base + 3 << '\n';
    nextVertexIndex += 8;
}

std::string makeHumanoidObj() {
    std::ostringstream out;
    out << "mtllib lowpoly_operator.mtl\n";
    out << "o lowpoly_operator\n";

    int nextVertexIndex = 1;
    constexpr std::array<HumanoidBox, 11> kBoxes{{
        {"boots", -0.10f, 0.00f, -0.11f, 0.10f, 0.12f, 0.08f},
        {"boots", -0.10f, 0.00f,  0.15f, 0.10f, 0.12f, 0.34f},
        {"pants", -0.11f, 0.12f, -0.08f, 0.11f, 0.72f, 0.06f},
        {"pants", -0.11f, 0.12f,  0.18f, 0.11f, 0.72f, 0.32f},
        {"vest", -0.22f, 0.72f, -0.18f, 0.22f, 1.26f, 0.38f},
        {"vest", -0.28f, 0.86f, -0.22f, 0.28f, 1.18f, 0.42f},
        {"skin", -0.13f, 1.26f, -0.06f, 0.13f, 1.40f, 0.28f},
        {"skin", -0.20f, 1.40f, -0.10f, 0.20f, 1.76f, 0.32f},
        {"skin", -0.39f, 0.86f, -0.05f, -0.23f, 1.18f, 0.09f},
        {"skin",  0.23f, 0.86f, -0.05f,  0.39f, 1.18f, 0.09f},
        {"visor", 0.14f, 1.48f,  0.04f, 0.22f, 1.64f, 0.20f},
    }};

    for (const HumanoidBox& box : kBoxes) {
        appendHumanoidBox(out, nextVertexIndex, box);
    }

    return out.str();
}

std::string makeHumanoidMtl() {
    return
        "newmtl skin\n"
        "Kd 0.86 0.72 0.62\n"
        "Ka 0.10 0.08 0.06\n"
        "Ks 0.02 0.02 0.02\n"
        "Ns 12.0\n"
        "\n"
        "newmtl vest\n"
        "Kd 0.34 0.38 0.42\n"
        "Ka 0.06 0.07 0.08\n"
        "Ks 0.02 0.02 0.02\n"
        "Ns 12.0\n"
        "\n"
        "newmtl pants\n"
        "Kd 0.22 0.27 0.31\n"
        "Ka 0.05 0.06 0.07\n"
        "Ks 0.02 0.02 0.02\n"
        "Ns 12.0\n"
        "\n"
        "newmtl boots\n"
        "Kd 0.10 0.11 0.12\n"
        "Ka 0.03 0.03 0.03\n"
        "Ks 0.01 0.01 0.01\n"
        "Ns 8.0\n"
        "\n"
        "newmtl visor\n"
        "Kd 0.18 0.30 0.36\n"
        "Ka 0.04 0.07 0.08\n"
        "Ks 0.05 0.05 0.05\n"
        "Ns 18.0\n";
}

std::vector<OpticType> commonOptics() {
    return {OpticType::RedDot, OpticType::X2, OpticType::X4, OpticType::X8};
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::filesystem::path normalizeCatalogPath(const std::filesystem::path& assetRoot,
                                           const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    std::error_code error;
    if (path.is_absolute()) {
        const std::filesystem::path relative = std::filesystem::relative(path, assetRoot, error);
        if (!error && !relative.empty()) {
            return relative.lexically_normal();
        }
    }

    std::string generic = path.lexically_normal().generic_string();
    if (generic.rfind("assets/", 0) == 0) {
        generic.erase(0, 7);
    }
    return std::filesystem::path(generic).lexically_normal();
}

std::filesystem::path objectThumbnailPathFor(const AssetManifest& manifest,
                                             const std::filesystem::path& modelPath,
                                             const std::filesystem::path& materialPath) {
    const std::string modelKey = modelPath.lexically_normal().generic_string();
    if (!modelKey.empty()) {
        const auto modelIt = std::find_if(manifest.models.begin(), manifest.models.end(), [&modelKey](const AssetManifestEntry& entry) {
            return entry.path.lexically_normal().generic_string() == modelKey;
        });
        if (modelIt != manifest.models.end() && !modelIt->thumbnailPath.empty()) {
            return modelIt->thumbnailPath;
        }
    }

    const std::string materialKey = materialPath.lexically_normal().generic_string();
    if (!materialKey.empty()) {
        const auto materialIt = std::find_if(manifest.materials.begin(), manifest.materials.end(), [&materialKey](const MaterialAssetEntry& entry) {
            return entry.materialPath.lexically_normal().generic_string() == materialKey;
        });
        if (materialIt != manifest.materials.end() && !materialIt->thumbnailPath.empty()) {
            return materialIt->thumbnailPath;
        }
    }
    return {};
}

const ObjectAssetDefinition* matchLegacyObjectAsset(const ObjectCatalog& catalog,
                                                    const std::filesystem::path& assetRoot,
                                                    const gameplay::MapProp& prop) {
    const std::string legacyId = lowerAscii(prop.id);
    const std::string modelPath = normalizeCatalogPath(assetRoot, prop.modelPath).generic_string();
    const std::string materialPath = normalizeCatalogPath(assetRoot, prop.materialPath).generic_string();

    if (!legacyId.empty()) {
        if (const auto* exact = catalog.find(prop.id); exact != nullptr) {
            return exact;
        }
    }

    int bestScore = 0;
    const ObjectAssetDefinition* best = nullptr;
    for (const auto& candidate : catalog.objects) {
        int score = 0;
        const std::string candidateId = lowerAscii(candidate.id);
        if (!modelPath.empty() && candidate.modelPath.lexically_normal().generic_string() == modelPath) {
            score += 80;
        }
        if (!materialPath.empty() && candidate.materialPath.lexically_normal().generic_string() == materialPath) {
            score += 40;
        }
        if (!legacyId.empty() && (candidateId == legacyId || candidateId.find(legacyId) != std::string::npos ||
                                  legacyId.find(candidateId) != std::string::npos)) {
            score += 30;
        }
        if (!legacyId.empty()) {
            if (legacyId.find("barrel") != std::string::npos && candidateId.find("barrel") != std::string::npos) {
                score += 20;
            }
            if (legacyId.find("crate") != std::string::npos && candidateId.find("crate") != std::string::npos) {
                score += 20;
            }
            if (legacyId.find("floor") != std::string::npos && candidateId.find("floor") != std::string::npos) {
                score += 20;
            }
            if ((legacyId.find("wall") != std::string::npos || legacyId.find("perimeter") != std::string::npos) &&
                candidateId.find("wall") != std::string::npos) {
                score += 20;
            }
        }
        if (score > bestScore) {
            bestScore = score;
            best = &candidate;
        }
    }
    return bestScore > 0 ? best : nullptr;
}

}  // namespace

void ContentDatabase::bootstrap(const std::filesystem::path& assetRoot) {
    assetRoot_ = assetRoot;
    createPlaceholderAssets(assetRoot);
    createMaterialProfiles(assetRoot);
    createWeaponCatalog(assetRoot);
    createCharacterCatalog(assetRoot);
    assetManifest_ = buildAssetManifest(assetRoot);
    createObjectCatalog(assetRoot);
    if (!writeAssetManifest(assetRoot, assetManifest_)) {
        spdlog::warn("[Content] Failed to write asset manifest: {}", (assetRoot / assetManifest_.manifestPath).generic_string());
    } else {
        spdlog::info("[Content] Asset manifest ready: models={} selectableModels={} textures={} selectableMaterials={}",
            assetManifest_.models.size(),
            assetManifest_.editorModels.size(),
            assetManifest_.textures.size(),
            assetManifest_.editorMaterials.size());
    }
}

void ContentDatabase::createPlaceholderAssets(const std::filesystem::path& assetRoot) {
    const auto generatedDir = assetRoot / "generated";
    const auto polyhavenDir = assetRoot / "source" / "polyhaven";
    util::FileSystem::ensureDirectory(generatedDir / "models");
    util::FileSystem::ensureDirectory(generatedDir / "textures");
    util::FileSystem::ensureDirectory(generatedDir / "materials");
    util::FileSystem::ensureDirectory(assetRoot / "source" / "characters");

    util::FileSystem::writeText(generatedDir / "models" / "crate.obj", makeCubeObj());
    util::FileSystem::writeText(generatedDir / "models" / "weapon_placeholder.obj", makeCubeObj());
    util::FileSystem::writeText(generatedDir / "models" / "lowpoly_operator.obj", makeHumanoidObj());
    util::FileSystem::writeText(generatedDir / "models" / "lowpoly_operator.mtl", makeHumanoidMtl());
    util::FileSystem::writeText(generatedDir / "materials" / "default.mat",
        "albedo=generated/textures/metal_gray.bmp\nroughness=0.65\nmetallic=0.3\n");
    util::FileSystem::writeText(generatedDir / "materials" / "polyhaven_concrete_wall_006.mat",
        "albedo=source/polyhaven/materials/concrete_wall_006/concrete_wall_006_diff_2k.jpg\n"
        "normal=source/polyhaven/materials/concrete_wall_006/concrete_wall_006_nor_gl_2k.jpg\n"
        "roughness=source/polyhaven/materials/concrete_wall_006/concrete_wall_006_rough_2k.jpg\n"
        "metallic=0.05\n");
    util::FileSystem::writeText(generatedDir / "materials" / "polyhaven_concrete_floor.mat",
        "albedo=source/polyhaven/materials/concrete_floor/concrete_floor_diff_2k.jpg\n"
        "normal=source/polyhaven/materials/concrete_floor/concrete_floor_nor_gl_2k.jpg\n"
        "roughness=source/polyhaven/materials/concrete_floor/concrete_floor_rough_2k.jpg\n"
        "metallic=0.02\n");
    util::FileSystem::writeText(generatedDir / "materials" / "polyhaven_painted_concrete.mat",
        "albedo=source/polyhaven/materials/painted_concrete/painted_concrete_diff_2k.jpg\n"
        "normal=source/polyhaven/materials/painted_concrete/painted_concrete_nor_gl_2k.jpg\n"
        "roughness=source/polyhaven/materials/painted_concrete/painted_concrete_rough_2k.jpg\n"
        "metallic=0.02\n");
    util::FileSystem::writeText(generatedDir / "materials" / "polyhaven_wooden_crate_02.mat",
        "albedo=source/polyhaven/models/wooden_crate_02/textures/wooden_crate_02_diff_1k.jpg\n"
        "normal=source/polyhaven/models/wooden_crate_02/textures/wooden_crate_02_nor_gl_1k.jpg\n"
        "arm=source/polyhaven/models/wooden_crate_02/textures/wooden_crate_02_arm_1k.jpg\n"
        "metallic=0.15\n");
    util::FileSystem::writeText(generatedDir / "materials" / "polyhaven_barrel_02.mat",
        "albedo=source/polyhaven/models/Barrel_02/textures/Barrel_02_diff_1k.jpg\n"
        "normal=source/polyhaven/models/Barrel_02/textures/Barrel_02_nor_gl_1k.jpg\n"
        "arm=source/polyhaven/models/Barrel_02/textures/Barrel_02_arm_1k.jpg\n"
        "metallic=0.35\n");
    util::FileSystem::writeText(generatedDir / "materials" / "classic64_box_shipping_01.mat",
        "albedo=source/itchio/classic64/Boxes/Materials/box_shipping_01.jpg\n"
        "roughness=0.78\n"
        "metallic=0.02\n");
    util::FileSystem::writeText(generatedDir / "materials" / "classic64_concrete_floor_01.mat",
        "albedo=source/itchio/classic64/Concrete/Materials/concrete_floor_01.jpg\n"
        "roughness=0.86\n"
        "metallic=0.01\n");
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_metal.mat",
        "albedo=generated/textures/metal_gray.bmp\nroughness=0.42\nmetallic=0.62\n");
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_polymer.mat",
        "albedo=generated/textures/polymer_black.bmp\nroughness=0.78\nmetallic=0.08\n");
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_wood.mat",
        "albedo=generated/textures/wood_walnut.bmp\nroughness=0.70\nmetallic=0.02\n");
    util::FileSystem::writeText(generatedDir / "materials" / "bomb_site_a.mat",
        "albedo=generated/textures/site_a_red.bmp\nroughness=0.76\nmetallic=0.02\n");
    util::FileSystem::writeText(generatedDir / "materials" / "bomb_site_b.mat",
        "albedo=generated/textures/site_b_blue.bmp\nroughness=0.76\nmetallic=0.02\n");
    util::FileSystem::writeBinary(generatedDir / "textures" / "metal_gray.bmp", makeBmp(132, 138, 148));
    util::FileSystem::writeBinary(generatedDir / "textures" / "polymer_black.bmp", makeBmp(54, 56, 62));
    util::FileSystem::writeBinary(generatedDir / "textures" / "wood_walnut.bmp", makeBmp(112, 74, 48));
    util::FileSystem::writeBinary(generatedDir / "textures" / "site_a_red.bmp", makeBmp(170, 72, 60));
    util::FileSystem::writeBinary(generatedDir / "textures" / "site_b_blue.bmp", makeBmp(66, 100, 176));
    util::FileSystem::writeBinary(generatedDir / "textures" / "smoke_soft.bmp", makeBmp(164, 174, 182));
    util::FileSystem::writeBinary(generatedDir / "textures" / "flash_white.bmp", makeBmp(235, 235, 228));
}

void ContentDatabase::createMaterialProfiles(const std::filesystem::path& assetRoot) {
    materials_.clear();
    const auto polyhavenRoot = assetRoot / "source" / "polyhaven";
    materials_.push_back(MaterialProfile{
        .id = "metal_gray",
        .displayName = "灰色金属",
        .albedoPath = assetRoot / "generated" / "textures" / "metal_gray.bmp",
        .normalPath = {},
        .roughnessPath = {},
    });
    materials_.push_back(MaterialProfile{
        .id = "polymer_black",
        .displayName = "黑色聚合物",
        .albedoPath = assetRoot / "generated" / "textures" / "polymer_black.bmp",
        .normalPath = {},
        .roughnessPath = {},
    });
    materials_.push_back(MaterialProfile{
        .id = "walnut_wood",
        .displayName = "胡桃木",
        .albedoPath = assetRoot / "generated" / "textures" / "wood_walnut.bmp",
        .normalPath = {},
        .roughnessPath = {},
    });
    if (util::FileSystem::exists(polyhavenRoot / "materials" / "concrete_wall_006" / "concrete_wall_006_diff_2k.jpg")) {
        materials_.push_back(MaterialProfile{
            .id = "concrete_wall_006",
            .displayName = "混凝土墙 006",
            .albedoPath = polyhavenRoot / "materials" / "concrete_wall_006" / "concrete_wall_006_diff_2k.jpg",
            .normalPath = polyhavenRoot / "materials" / "concrete_wall_006" / "concrete_wall_006_nor_gl_2k.jpg",
            .roughnessPath = polyhavenRoot / "materials" / "concrete_wall_006" / "concrete_wall_006_rough_2k.jpg",
        });
    }
    if (util::FileSystem::exists(polyhavenRoot / "materials" / "concrete_floor" / "concrete_floor_diff_2k.jpg")) {
        materials_.push_back(MaterialProfile{
            .id = "concrete_floor",
            .displayName = "混凝土地面",
            .albedoPath = polyhavenRoot / "materials" / "concrete_floor" / "concrete_floor_diff_2k.jpg",
            .normalPath = polyhavenRoot / "materials" / "concrete_floor" / "concrete_floor_nor_gl_2k.jpg",
            .roughnessPath = polyhavenRoot / "materials" / "concrete_floor" / "concrete_floor_rough_2k.jpg",
        });
    }
    if (util::FileSystem::exists(polyhavenRoot / "materials" / "painted_concrete" / "painted_concrete_diff_2k.jpg")) {
        materials_.push_back(MaterialProfile{
            .id = "painted_concrete",
            .displayName = "涂装混凝土",
            .albedoPath = polyhavenRoot / "materials" / "painted_concrete" / "painted_concrete_diff_2k.jpg",
            .normalPath = polyhavenRoot / "materials" / "painted_concrete" / "painted_concrete_nor_gl_2k.jpg",
            .roughnessPath = polyhavenRoot / "materials" / "painted_concrete" / "painted_concrete_rough_2k.jpg",
        });
    }
    if (util::FileSystem::exists(assetRoot / "source" / "itchio" / "classic64" / "Boxes" / "Materials" / "box_shipping_01.jpg")) {
        materials_.push_back(MaterialProfile{
            .id = "classic64_box_shipping_01",
            .displayName = "Classic64 货运箱",
            .albedoPath = assetRoot / "source" / "itchio" / "classic64" / "Boxes" / "Materials" / "box_shipping_01.jpg",
            .normalPath = {},
            .roughnessPath = {},
        });
    }
    if (util::FileSystem::exists(assetRoot / "source" / "itchio" / "classic64" / "Concrete" / "Materials" / "concrete_floor_01.jpg")) {
        materials_.push_back(MaterialProfile{
            .id = "classic64_concrete_floor_01",
            .displayName = "Classic64 混凝土地面",
            .albedoPath = assetRoot / "source" / "itchio" / "classic64" / "Concrete" / "Materials" / "concrete_floor_01.jpg",
            .normalPath = {},
            .roughnessPath = {},
        });
    }
}

void ContentDatabase::createWeaponCatalog(const std::filesystem::path& assetRoot) {
    const auto model = assetRoot / "generated" / "models" / "weapon_placeholder.obj";
    const auto metal = assetRoot / "generated" / "textures" / "metal_gray.bmp";
    const auto polymer = assetRoot / "generated" / "textures" / "polymer_black.bmp";
    const auto walnut = assetRoot / "generated" / "textures" / "wood_walnut.bmp";
    const auto material = assetRoot / "generated" / "materials" / "default.mat";
    const auto quaterniusRoot = assetRoot / "source" / "weapons" / "quaternius_ultimate_guns" / "OBJ";
    const auto quaterniusMetal = assetRoot / "generated" / "materials" / "quaternius_metal.mat";
    const auto quaterniusPolymer = assetRoot / "generated" / "materials" / "quaternius_polymer.mat";
    const auto quaterniusWood = assetRoot / "generated" / "materials" / "quaternius_wood.mat";

    const auto chooseModel = [&](const std::filesystem::path& candidate) {
        return util::FileSystem::exists(candidate) ? candidate : model;
    };

    weapons_.clear();
    const AttachmentSlot opticSlot{"optic", commonOptics()};

    weapons_.push_back({"ak12", "AK-12", WeaponCategory::Rifle, 30, 90, 34.0f, 650.0f, 2.4f, 1.0f, 1.8f, 0.8f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "AssaultRifle_1.obj"), walnut, quaterniusWood}});
    weapons_.push_back({"m4a1", "M4A1", WeaponCategory::Rifle, 30, 90, 31.0f, 720.0f, 2.0f, 0.8f, 1.4f, 0.6f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "AssaultRifle_2.obj"), metal, quaterniusMetal}});
    weapons_.push_back({"scarh", "SCAR-H", WeaponCategory::Rifle, 20, 80, 39.0f, 600.0f, 2.5f, 1.1f, 2.1f, 0.9f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "AssaultRifle2_1.obj"), metal, quaterniusMetal}});
    weapons_.push_back({"awm", "AWM", WeaponCategory::SniperRifle, 5, 20, 92.0f, 48.0f, 4.8f, 0.15f, 3.8f, 0.5f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "SniperRifle_1.obj"), metal, quaterniusMetal}});
    weapons_.push_back({"svd", "SVD", WeaponCategory::SniperRifle, 10, 40, 79.0f, 150.0f, 4.0f, 0.35f, 2.6f, 0.6f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "SniperRifle_2.obj"), walnut, quaterniusWood}});
    weapons_.push_back({"mp5", "MP5", WeaponCategory::SubmachineGun, 30, 120, 24.0f, 850.0f, 1.8f, 0.9f, 1.0f, 0.5f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "SubmachineGun_1.obj"), polymer, quaterniusPolymer}});
    weapons_.push_back({"vector", "Vector", WeaponCategory::SubmachineGun, 25, 100, 22.0f, 950.0f, 1.7f, 0.8f, 1.1f, 0.6f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "SubmachineGun_4.obj"), polymer, quaterniusPolymer}});
    weapons_.push_back({"m870", "M870", WeaponCategory::Shotgun, 8, 32, 110.0f, 72.0f, 5.5f, 3.2f, 2.9f, 1.2f, false, false, {},
        {chooseModel(quaterniusRoot / "Shotgun_1.obj"), walnut, quaterniusWood}});
    weapons_.push_back({"saiga12", "Saiga-12", WeaponCategory::Shotgun, 10, 40, 98.0f, 240.0f, 5.0f, 2.8f, 2.3f, 1.1f, false, false, {opticSlot},
        {chooseModel(quaterniusRoot / "Shotgun_4.obj"), polymer, quaterniusPolymer}});
    weapons_.push_back({"combat_knife", "战术匕首", WeaponCategory::Melee, 1, 0, 55.0f, 90.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false, {},
        {chooseModel(quaterniusRoot / "Accessories" / "Bayonet.obj"), metal, quaterniusMetal}});
    weapons_.push_back({"frag", "破片手雷", WeaponCategory::Grenade, 1, 2, 120.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, true, {}, {model, metal, material}});
    weapons_.push_back({"flashbang", "闪光弹", WeaponCategory::Grenade, 1, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, true, {}, {model, assetRoot / "generated" / "textures" / "flash_white.bmp", material}});
    weapons_.push_back({"smoke", "烟雾弹", WeaponCategory::Grenade, 1, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, true, {}, {model, assetRoot / "generated" / "textures" / "smoke_soft.bmp", material}});
}

void ContentDatabase::createCharacterCatalog(const std::filesystem::path& assetRoot) {
    characters_.clear();

    const auto generatedModel = assetRoot / "generated" / "models" / "lowpoly_operator.obj";
    const auto demeliusModel = assetRoot / "source" / "characters" / "demelius_low_poly" / "Low-poly_character.glb";
    const auto superheroMaleModel = assetRoot / "source" / "itchio" / "universal_base_characters_standard" /
        "Base Characters" / "Godot - UE" / "Superhero_Male_FullBody.gltf";
    const auto superheroFemaleModel = assetRoot / "source" / "itchio" / "universal_base_characters_standard" /
        "Base Characters" / "Godot - UE" / "Superhero_Female_FullBody.gltf";
    const bool hasDemeliusModel = util::FileSystem::exists(demeliusModel);
    const bool hasSuperheroMaleModel = util::FileSystem::exists(superheroMaleModel);
    const bool hasSuperheroFemaleModel = util::FileSystem::exists(superheroFemaleModel);

    characters_.push_back(CharacterDefinition{
        .id = "default_operator",
        .displayName = hasDemeliusModel ? "低模干员（外部资源）" : "低模干员（内置）",
        .assets = {
            .modelPath = hasDemeliusModel ? demeliusModel : generatedModel,
            .albedoPath = {},
            .materialPath = {},
        },
        .modelScale = 1.0f,
        .yawOffsetRadians = 1.57079632679f,
    });

    if (hasSuperheroMaleModel) {
        characters_.push_back(CharacterDefinition{
            .id = "universal_superhero_male",
            .displayName = "通用角色 男",
            .assets = {
                .modelPath = superheroMaleModel,
                .albedoPath = {},
                .materialPath = {},
            },
            .modelScale = 1.0f,
            .yawOffsetRadians = 1.57079632679f,
        });
    }

    if (hasSuperheroFemaleModel) {
        characters_.push_back(CharacterDefinition{
            .id = "universal_superhero_female",
            .displayName = "通用角色 女",
            .assets = {
                .modelPath = superheroFemaleModel,
                .albedoPath = {},
                .materialPath = {},
            },
            .modelScale = 1.0f,
            .yawOffsetRadians = 1.57079632679f,
        });
    }
}

void ContentDatabase::createObjectCatalog(const std::filesystem::path& assetRoot) {
    ObjectCatalog catalog = buildDefaultObjectCatalog(assetRoot, assetManifest_);
    const std::filesystem::path catalogPath = assetRoot / catalog.catalogPath;
    if (util::FileSystem::exists(catalogPath)) {
        const ObjectCatalog loadedCatalog = loadObjectCatalog(assetRoot, catalogPath);
        for (const auto& object : loadedCatalog.objects) {
            catalog.upsert(object);
        }
    }

    objectCatalog_ = std::move(catalog);
    if (!persistObjectCatalog()) {
        spdlog::warn("[Content] Failed to write object catalog: {}", (assetRoot / objectCatalog_.catalogPath).generic_string());
    } else {
        spdlog::info("[Content] Object catalog ready: {} objects, {} editor categories",
            objectCatalog_.objects.size(),
            objectCatalog_.categoryCount());
    }
}

bool ContentDatabase::persistObjectCatalog() const {
    if (assetRoot_.empty()) {
        return false;
    }
    return writeObjectCatalog(assetRoot_, objectCatalog_);
}

const CharacterDefinition* ContentDatabase::findCharacter(const std::string_view id) const {
    const auto it = std::find_if(characters_.begin(), characters_.end(), [id](const CharacterDefinition& character) {
        return character.id == id;
    });
    return it != characters_.end() ? &*it : nullptr;
}

const ObjectAssetDefinition* ContentDatabase::findObjectAsset(const std::string_view id) const {
    return objectCatalog_.find(id);
}

const CharacterDefinition* ContentDatabase::defaultCharacter() const {
    return characters_.empty() ? nullptr : &characters_.front();
}

gameplay::MapProp ContentDatabase::instantiateMapProp(const std::string_view objectId,
                                                      const util::Vec3& position,
                                                      const util::Vec3& rotationDegrees,
                                                      const util::Vec3& scale) const {
    gameplay::MapProp prop;
    prop.id = std::string(objectId);
    prop.position = position;
    prop.rotationDegrees = rotationDegrees;
    prop.scale = scale;
    resolveMapProp(prop);
    return prop;
}

bool ContentDatabase::resolveMapProp(gameplay::MapProp& prop) const {
    const ObjectAssetDefinition* definition = nullptr;
    if (!prop.id.empty()) {
        definition = findObjectAsset(prop.id);
    }
    if (definition == nullptr) {
        definition = matchLegacyObjectAsset(objectCatalog_, assetRoot_, prop);
    }
    if (definition == nullptr) {
        if (prop.label.empty()) {
            prop.label = prop.id.empty() ? "道具" : prop.id;
        }
        return false;
    }

    prop.id = definition->id;
    prop.label = definition->label;
    prop.category = definition->category;
    prop.modelPath = definition->modelPath.empty()
        ? std::filesystem::path{}
        : (definition->modelPath.is_absolute() ? definition->modelPath : (assetRoot_ / definition->modelPath));
    prop.materialPath = definition->materialPath.empty()
        ? std::filesystem::path{}
        : (definition->materialPath.is_absolute() ? definition->materialPath : (assetRoot_ / definition->materialPath));
    prop.collisionHalfExtents = definition->collisionHalfExtents;
    prop.collisionCenterOffset = definition->collisionCenterOffset;
    prop.previewColor = definition->previewColor;
    prop.cylindricalFootprint = definition->cylindricalFootprint;
    return true;
}

void ContentDatabase::resolveMapData(gameplay::MapData& map) const {
    for (auto& prop : map.props) {
        resolveMapProp(prop);
    }
}

bool ContentDatabase::upsertObjectAsset(ObjectAssetDefinition definition) {
    definition.modelPath = normalizeCatalogPath(assetRoot_, definition.modelPath);
    definition.materialPath = normalizeCatalogPath(assetRoot_, definition.materialPath);
    if (definition.thumbnailPath.empty()) {
        definition.thumbnailPath = objectThumbnailPathFor(assetManifest_, definition.modelPath, definition.materialPath);
    } else {
        definition.thumbnailPath = normalizeCatalogPath(assetRoot_, definition.thumbnailPath);
    }
    if (!objectCatalog_.upsert(std::move(definition))) {
        return false;
    }
    return persistObjectCatalog();
}

bool ContentDatabase::removeObjectAsset(const std::string_view id) {
    if (!objectCatalog_.remove(id)) {
        return false;
    }
    return persistObjectCatalog();
}

}  // namespace mycsg::content
