#pragma once

#include "content/GameContent.h"
#include "gameplay/MapData.h"
#include "util/MathTypes.h"

#include <entt/entt.hpp>

#include <string>

namespace mycsg::gameplay {

using Entity = entt::entity;
inline constexpr Entity kNullEntity = entt::null;

struct TransformComponent {
    util::Vec3 position{};
};

struct VelocityComponent {
    util::Vec3 linear{};
};

struct HealthComponent {
    float current = 100.0f;
    float maximum = 100.0f;
};

struct TeamComponent {
    Team value = Team::Neutral;
};

struct PlayerIdentityComponent {
    std::string id;
    std::string displayName;
};

struct PlayerLoadoutComponent {
    std::string primaryWeaponId;
    std::string secondaryWeaponId;
    std::string tacticalGrenadeId;
    std::string lethalGrenadeId;
    content::OpticType optic = content::OpticType::RedDot;
};

struct PlayerTag {};
struct BotControlledComponent {};

// Kept separate even before full gameplay usage so battle royale / utility expansion
// can add item-specific systems without inflating the player state blob.
struct ThrowableInventoryComponent {
    int fragCount = 0;
    int flashCount = 0;
    int smokeCount = 0;
};

struct WeaponStateComponent {
    std::string activeWeaponId;
    int ammoInMagazine = 0;
    int reserveAmmo = 0;
};

struct ProjectileComponent {
    std::string itemId;
    float fuseSeconds = 0.0f;
    float blastRadius = 0.0f;
};

struct AiAgentComponent {
    float reactionDelaySeconds = 0.25f;
};

struct VehicleComponent {
    float maxSpeed = 0.0f;
    float health = 0.0f;
};

struct PlayerComponents {
    Entity entity = kNullEntity;
    PlayerIdentityComponent& identity;
    TeamComponent& team;
    TransformComponent& transform;
    VelocityComponent& velocity;
    HealthComponent& health;
    PlayerLoadoutComponent& loadout;
    bool botControlled = false;
};

struct ConstPlayerComponents {
    Entity entity = kNullEntity;
    const PlayerIdentityComponent& identity;
    const TeamComponent& team;
    const TransformComponent& transform;
    const VelocityComponent& velocity;
    const HealthComponent& health;
    const PlayerLoadoutComponent& loadout;
    bool botControlled = false;
};

}  // namespace mycsg::gameplay
