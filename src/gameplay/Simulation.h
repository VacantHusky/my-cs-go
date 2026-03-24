#pragma once

#include "content/GameContent.h"
#include "gameplay/MapData.h"
#include "util/MathTypes.h"

#include <string>
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
    void addPlayer(PlayerState player);
    void tick(float deltaSeconds);

    const MatchRules& rules() const { return rules_; }
    const MapData& map() const { return map_; }
    const std::vector<PlayerState>& players() const { return players_; }
    std::vector<PlayerState>& players() { return players_; }
    float elapsedSeconds() const { return elapsedSeconds_; }

private:
    MapData map_;
    MatchRules rules_;
    std::vector<PlayerState> players_;
    float elapsedSeconds_ = 0.0f;
};

SimulationWorld makeOfflinePracticeWorld(const MapData& map);

}  // namespace mycsg::gameplay
