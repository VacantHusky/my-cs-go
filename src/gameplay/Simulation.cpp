#include "gameplay/Simulation.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>

namespace mycsg::gameplay {

SimulationWorld::SimulationWorld(MapData map) : map_(std::move(map)) {}

void SimulationWorld::setRules(MatchRules rules) {
    rules_ = std::move(rules);
}

Entity SimulationWorld::addPlayer(PlayerState player) {
    Entity entity = registry_.create();
    registry_.emplace<PlayerTag>(entity);
    registry_.emplace<PlayerIdentityComponent>(entity, std::move(player.id), std::move(player.displayName));
    registry_.emplace<TeamComponent>(entity, player.team);
    registry_.emplace<TransformComponent>(entity, player.position);
    registry_.emplace<VelocityComponent>(entity, player.velocity);
    registry_.emplace<HealthComponent>(entity, player.health, std::max(100.0f, player.health));
    registry_.emplace<PlayerLoadoutComponent>(entity,
        std::move(player.loadout.primaryWeaponId),
        std::move(player.loadout.secondaryWeaponId),
        std::move(player.loadout.tacticalGrenadeId),
        std::move(player.loadout.lethalGrenadeId),
        player.loadout.optic);

    if (player.botControlled) {
        registry_.emplace<BotControlledComponent>(entity);
        registry_.emplace<AiAgentComponent>(entity);
    }

    playerEntities_.push_back(entity);
    playerCacheDirty_ = true;
    return entity;
}

Entity SimulationWorld::upsertPlayer(PlayerState player) {
    if (const Entity existing = findPlayerById(player.id); existing != kNullEntity) {
        withPlayer(existing, [&](PlayerComponents& current) {
            current.identity.displayName = player.displayName;
            current.team.value = player.team;
            current.transform.position = player.position;
            current.velocity.linear = player.velocity;
            current.health.current = std::clamp(player.health, 0.0f, current.health.maximum);
            current.loadout.primaryWeaponId = player.loadout.primaryWeaponId;
            current.loadout.secondaryWeaponId = player.loadout.secondaryWeaponId;
            current.loadout.tacticalGrenadeId = player.loadout.tacticalGrenadeId;
            current.loadout.lethalGrenadeId = player.loadout.lethalGrenadeId;
            current.loadout.optic = player.loadout.optic;
        });

        if (registry_.valid(existing)) {
            if (player.botControlled) {
                if (!registry_.all_of<BotControlledComponent>(existing)) {
                    registry_.emplace<BotControlledComponent>(existing);
                }
                if (!registry_.all_of<AiAgentComponent>(existing)) {
                    registry_.emplace<AiAgentComponent>(existing);
                }
            } else {
                if (registry_.all_of<BotControlledComponent>(existing)) {
                    registry_.remove<BotControlledComponent>(existing);
                }
                if (registry_.all_of<AiAgentComponent>(existing)) {
                    registry_.remove<AiAgentComponent>(existing);
                }
            }
        }

        playerCacheDirty_ = true;
        return existing;
    }

    return addPlayer(std::move(player));
}

void SimulationWorld::tick(const float deltaSeconds) {
    elapsedSeconds_ += deltaSeconds;

    auto movingEntities = registry_.view<TransformComponent, VelocityComponent>();
    for (auto entity : movingEntities) {
        auto& transform = movingEntities.get<TransformComponent>(entity);
        const auto& velocity = movingEntities.get<VelocityComponent>(entity);
        transform.position.x += velocity.linear.x * deltaSeconds;
        transform.position.y += velocity.linear.y * deltaSeconds;
        transform.position.z += velocity.linear.z * deltaSeconds;
    }

    playerCacheDirty_ = true;
}

void SimulationWorld::replacePlayers(const std::vector<PlayerState>& players) {
    for (const Entity entity : playerEntities_) {
        if (registry_.valid(entity)) {
            registry_.destroy(entity);
        }
    }

    playerEntities_.clear();
    playerCache_.clear();
    playerCacheDirty_ = true;

    for (const PlayerState& player : players) {
        addPlayer(player);
    }
}

const std::vector<PlayerState>& SimulationWorld::players() const {
    if (playerCacheDirty_) {
        rebuildPlayerCache();
    }
    return playerCache_;
}

Entity SimulationWorld::firstPlayerEntity() const {
    for (const Entity entity : playerEntities_) {
        if (valid(entity)) {
            return entity;
        }
    }
    return kNullEntity;
}

Entity SimulationWorld::findPlayerById(const std::string_view id) const {
    for (const Entity entity : playerEntities_) {
        if (!valid(entity)) {
            continue;
        }
        const auto& identity = registry_.get<PlayerIdentityComponent>(entity);
        if (identity.id == id) {
            return entity;
        }
    }
    return kNullEntity;
}

bool SimulationWorld::valid(const Entity entity) const {
    return entity != kNullEntity &&
           registry_.valid(entity) &&
           registry_.all_of<PlayerTag, PlayerIdentityComponent, TeamComponent, TransformComponent,
                            VelocityComponent, HealthComponent, PlayerLoadoutComponent>(entity);
}

bool SimulationWorld::withPlayer(const Entity entity,
                                 const std::function<void(PlayerComponents&)>& func) {
    if (!valid(entity)) {
        return false;
    }

    PlayerComponents player{
        entity,
        registry_.get<PlayerIdentityComponent>(entity),
        registry_.get<TeamComponent>(entity),
        registry_.get<TransformComponent>(entity),
        registry_.get<VelocityComponent>(entity),
        registry_.get<HealthComponent>(entity),
        registry_.get<PlayerLoadoutComponent>(entity),
        registry_.all_of<BotControlledComponent>(entity),
    };
    func(player);
    playerCacheDirty_ = true;
    return true;
}

bool SimulationWorld::withPlayer(const Entity entity,
                                 const std::function<void(const ConstPlayerComponents&)>& func) const {
    if (!valid(entity)) {
        return false;
    }

    ConstPlayerComponents player{
        entity,
        registry_.get<PlayerIdentityComponent>(entity),
        registry_.get<TeamComponent>(entity),
        registry_.get<TransformComponent>(entity),
        registry_.get<VelocityComponent>(entity),
        registry_.get<HealthComponent>(entity),
        registry_.get<PlayerLoadoutComponent>(entity),
        registry_.all_of<BotControlledComponent>(entity),
    };
    func(player);
    return true;
}

void SimulationWorld::respawnPlayer(const Entity entity,
                                    const util::Vec3& position,
                                    const util::Vec3& velocity,
                                    const float health) {
    if (!valid(entity)) {
        return;
    }

    auto& transform = registry_.get<TransformComponent>(entity);
    auto& movement = registry_.get<VelocityComponent>(entity);
    auto& hp = registry_.get<HealthComponent>(entity);
    transform.position = position;
    movement.linear = velocity;
    hp.current = std::clamp(health, 0.0f, hp.maximum);
    playerCacheDirty_ = true;
}

bool SimulationWorld::removePlayerById(const std::string_view id) {
    const Entity entity = findPlayerById(id);
    if (entity == kNullEntity) {
        return false;
    }

    registry_.destroy(entity);
    playerEntities_.erase(std::remove(playerEntities_.begin(), playerEntities_.end(), entity), playerEntities_.end());
    playerCacheDirty_ = true;
    return true;
}

void SimulationWorld::rebuildPlayerCache() const {
    playerCache_.clear();
    playerCache_.reserve(playerEntities_.size());

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
        playerCache_.push_back(PlayerState{
            .id = identity.id,
            .displayName = identity.displayName,
            .team = team.value,
            .position = transform.position,
            .velocity = velocity.linear,
            .health = health.current,
            .botControlled = registry_.all_of<BotControlledComponent>(entity),
            .loadout = {
                loadout.primaryWeaponId,
                loadout.secondaryWeaponId,
                loadout.tacticalGrenadeId,
                loadout.lethalGrenadeId,
                loadout.optic,
            },
        });
    }

    playerCacheDirty_ = false;
}

SimulationWorld makeOfflinePracticeWorld(const MapData& map) {
    spdlog::info("[Simulation] 正在创建离线训练场规则...");
    SimulationWorld world(map);
    world.setRules(MatchRules{
        .mode = MatchMode::BombDefusal,
        .roundTimeSeconds = 115,
        .buyTimeSeconds = 20,
        .maxRounds = 24,
        .friendlyFire = false,
    });

    spdlog::info("[Simulation] 正在创建角色: 本地玩家");
    world.addPlayer(PlayerState{
        .id = "p0",
        .displayName = "本地玩家",
        .team = Team::Attackers,
        .position = {3.0f, 1.0f, 3.0f},
        .velocity = {0.0f, 0.0f, 0.0f},
        .loadout = {"ak12", "combat_knife", "flashbang", "frag", content::OpticType::RedDot},
    });
    spdlog::info("[Simulation] 角色创建完毕: 本地玩家");

    spdlog::info("[Simulation] 正在创建角色: 守方机器人");
    world.addPlayer(PlayerState{
        .id = "bot_ct_0",
        .displayName = "守方机器人",
        .team = Team::Defenders,
        .position = {20.0f, 1.0f, 20.0f},
        .velocity = {-0.1f, 0.0f, 0.0f},
        .botControlled = true,
        .loadout = {"m4a1", "combat_knife", "flashbang", "smoke", content::OpticType::X2},
    });
    spdlog::info("[Simulation] 角色创建完毕: 守方机器人");
    spdlog::info("[Simulation] 离线训练场初始化完成。");
    return world;
}

}  // namespace mycsg::gameplay
