#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mycsg::content {

enum class WeaponCategory {
    Rifle,
    SniperRifle,
    SubmachineGun,
    Shotgun,
    Melee,
    Grenade
};

enum class OpticType {
    IronSight,
    RedDot,
    X2,
    X4,
    X8
};

struct AttachmentSlot {
    std::string id;
    std::vector<OpticType> supportedOptics;
};

struct AssetBinding {
    std::filesystem::path modelPath;
    std::filesystem::path albedoPath;
    std::filesystem::path materialPath;
};

struct WeaponDefinition {
    std::string id;
    std::string displayName;
    WeaponCategory category = WeaponCategory::Rifle;
    int magazineSize = 30;
    int reserveAmmo = 90;
    float damage = 30.0f;
    float fireRateRpm = 600.0f;
    float hipSpread = 2.0f;
    float aimSpread = 0.9f;
    float recoilPitch = 1.5f;
    float recoilYaw = 0.7f;
    bool throwable = false;
    bool hasSplashEffect = false;
    std::vector<AttachmentSlot> attachments;
    AssetBinding assets;
};

struct MaterialProfile {
    std::string id;
    std::string displayName;
    std::filesystem::path albedoPath;
    std::filesystem::path normalPath;
    std::filesystem::path roughnessPath;
};

struct CharacterDefinition {
    std::string id;
    std::string displayName;
    AssetBinding assets;
    float modelScale = 1.0f;
    float yawOffsetRadians = 0.0f;
};

class ContentDatabase {
public:
    void bootstrap(const std::filesystem::path& assetRoot);

    const std::vector<WeaponDefinition>& weapons() const { return weapons_; }
    const std::vector<MaterialProfile>& materials() const { return materials_; }
    const std::vector<CharacterDefinition>& characters() const { return characters_; }
    const CharacterDefinition* findCharacter(std::string_view id) const;
    const CharacterDefinition* defaultCharacter() const;

private:
    void createPlaceholderAssets(const std::filesystem::path& assetRoot);
    void createMaterialProfiles(const std::filesystem::path& assetRoot);
    void createWeaponCatalog(const std::filesystem::path& assetRoot);
    void createCharacterCatalog(const std::filesystem::path& assetRoot);

    std::vector<WeaponDefinition> weapons_;
    std::vector<MaterialProfile> materials_;
    std::vector<CharacterDefinition> characters_;
};

}  // namespace mycsg::content
