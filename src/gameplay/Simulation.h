#pragma once

#include "content/GameContent.h"
#include "gameplay/EcsComponents.h"
#include "gameplay/MapData.h"
#include "util/MathTypes.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mycsg::gameplay {

enum class MatchMode {
    BombDefusal,
    TeamDeathmatch,
    Sandbox,
    BattleRoyalePrototype
};

struct PlayerLoadout {
    std::string primaryWeaponId;
    std::string secondaryWeaponId;
    std::string tacticalGrenadeId;
    std::string lethalGrenadeId;
    content::OpticType optic = content::OpticType::RedDot;
};

struct PlayerState {
    std::string id;
    std::string displayName;
    Team team = Team::Neutral;
    util::Vec3 position;
    util::Vec3 velocity;
    float health = 100.0f;
    bool botControlled = false;
    PlayerLoadout loadout;
};

struct MatchRules {
    MatchMode mode = MatchMode::BombDefusal;
    int roundTimeSeconds = 115;
    int buyTimeSeconds = 20;
    int maxRounds = 24;
    bool friendlyFire = false;
};

class SimulationWorld {
public:
    explicit SimulationWorld(MapData map = {});

    void setRules(MatchRules rules);
    Entity addPlayer(PlayerState player);
    Entity upsertPlayer(PlayerState player);
    void tick(float deltaSeconds);
    void replacePlayers(const std::vector<PlayerState>& players);
    void setElapsedSeconds(float elapsedSeconds) { elapsedSeconds_ = elapsedSeconds; }

    const MatchRules& rules() const { return rules_; }
    const MapData& map() const { return map_; }
    const std::vector<PlayerState>& players() const;
    float elapsedSeconds() const { return elapsedSeconds_; }
    Entity firstPlayerEntity() const;
    Entity findPlayerById(std::string_view id) const;
    bool valid(Entity entity) const;
    bool withPlayer(Entity entity, const std::function<void(PlayerComponents&)>& func);
    bool withPlayer(Entity entity, const std::function<void(const ConstPlayerComponents&)>& func) const;
    bool removePlayerById(std::string_view id);
    void respawnPlayer(Entity entity,
                       const util::Vec3& position,
                       const util::Vec3& velocity,
                       float health = 100.0f);

    template <typename Func>
    void forEachPlayer(Func&& func) {
        for (const Entity entity : playerEntities_) {
            if (!valid(entity)) {
                continue;
            }

            auto& identity = registry_.get<PlayerIdentityComponent>(entity);
            auto& team = registry_.get<TeamComponent>(entity);
            auto& transform = registry_.get<TransformComponent>(entity);
            auto& velocity = registry_.get<VelocityComponent>(entity);
            auto& health = registry_.get<HealthComponent>(entity);
            auto& loadout = registry_.get<PlayerLoadoutComponent>(entity);
            PlayerComponents player{
                entity,
                identity,
                team,
                transform,
                velocity,
                health,
                loadout,
                registry_.all_of<BotControlledComponent>(entity),
            };
            std::invoke(std::forward<Func>(func), player);
        }
        playerCacheDirty_ = true;
    }

    template <typename Func>
    void forEachPlayer(Func&& func) const {
        for (const Entity entity : playerEntities_) {
            if (!valid(entity)) {
                continue;
            }

            const auto& identity = registry_.get<PlayerIdentityComponent>(entity);
            const auto& team = registry_.get<TeamComponent>(entity);
            const auto& transform = registry_.get<TransformComponent>(entity);
            const auto& velocity = registry_.get<VelocityComponent>(entity);
            const auto& health = registry_.get<HealthComponent>(entity);
            const auto& loadout = registry_.get<PlayerLoadoutComponent>(entity);
            ConstPlayerComponents player{
                entity,
                identity,
                team,
                transform,
                velocity,
                health,
                loadout,
                registry_.all_of<BotControlledComponent>(entity),
            };
            std::invoke(std::forward<Func>(func), player);
        }
    }

    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }

private:
    void rebuildPlayerCache() const;

    MapData map_;
    MatchRules rules_;
    entt::registry registry_;
    std::vector<Entity> playerEntities_;
    mutable std::vector<PlayerState> playerCache_;
    mutable bool playerCacheDirty_ = true;
    float elapsedSeconds_ = 0.0f;
};

SimulationWorld makeOfflinePracticeWorld(const MapData& map);

}  // namespace mycsg::gameplay
