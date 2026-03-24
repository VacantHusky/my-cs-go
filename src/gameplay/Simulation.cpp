#include "gameplay/Simulation.h"

#include <spdlog/spdlog.h>

#include <iostream>
#include <utility>

namespace mycsg::gameplay {

SimulationWorld::SimulationWorld(MapData map) : map_(std::move(map)) {}

void SimulationWorld::setRules(MatchRules rules) {
    rules_ = std::move(rules);
}

void SimulationWorld::addPlayer(PlayerState player) {
    players_.push_back(std::move(player));
}

void SimulationWorld::tick(const float deltaSeconds) {
    elapsedSeconds_ += deltaSeconds;
    for (auto& player : players_) {
        player.position.x += player.velocity.x * deltaSeconds;
        player.position.y += player.velocity.y * deltaSeconds;
        player.position.z += player.velocity.z * deltaSeconds;
    }
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
