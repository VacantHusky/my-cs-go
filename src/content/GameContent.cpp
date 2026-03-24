#include "content/GameContent.h"

#include "util/FileSystem.h"

#include <algorithm>
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

std::vector<OpticType> commonOptics() {
    return {OpticType::RedDot, OpticType::X2, OpticType::X4, OpticType::X8};
}

}  // namespace

void ContentDatabase::bootstrap(const std::filesystem::path& assetRoot) {
    createPlaceholderAssets(assetRoot);
    createMaterialProfiles(assetRoot);
    createWeaponCatalog(assetRoot);
}

void ContentDatabase::createPlaceholderAssets(const std::filesystem::path& assetRoot) {
    const auto generatedDir = assetRoot / "generated";
    const auto polyhavenDir = assetRoot / "source" / "polyhaven";
    util::FileSystem::ensureDirectory(generatedDir / "models");
    util::FileSystem::ensureDirectory(generatedDir / "textures");
    util::FileSystem::ensureDirectory(generatedDir / "materials");

    util::FileSystem::writeText(generatedDir / "models" / "crate.obj", makeCubeObj());
    util::FileSystem::writeText(generatedDir / "models" / "weapon_placeholder.obj", makeCubeObj());
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
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_metal.mat",
        "albedo=generated/textures/metal_gray.bmp\nroughness=0.42\nmetallic=0.62\n");
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_polymer.mat",
        "albedo=generated/textures/polymer_black.bmp\nroughness=0.78\nmetallic=0.08\n");
    util::FileSystem::writeText(generatedDir / "materials" / "quaternius_wood.mat",
        "albedo=generated/textures/wood_walnut.bmp\nroughness=0.70\nmetallic=0.02\n");
    util::FileSystem::writeBinary(generatedDir / "textures" / "metal_gray.bmp", makeBmp(132, 138, 148));
    util::FileSystem::writeBinary(generatedDir / "textures" / "polymer_black.bmp", makeBmp(54, 56, 62));
    util::FileSystem::writeBinary(generatedDir / "textures" / "wood_walnut.bmp", makeBmp(112, 74, 48));
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

}  // namespace mycsg::content
