#pragma once

#include "gameplay/MapData.h"
#include "util/MathTypes.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mycsg::gameplay {

enum class PhysicsMovementEventType {
    Jumped,
    Landed,
    SteppedUp,
};

struct PhysicsMovementEvent {
    PhysicsMovementEventType type = PhysicsMovementEventType::Jumped;
    float intensity = 0.0f;
    float height = 0.0f;
};

struct PhysicsDetonationEvent {
    std::string itemId;
    util::Vec3 position{};
    float effectRadius = 0.0f;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    bool initialize(const std::filesystem::path& assetRoot,
                    const MapData& map,
                    const util::Vec3& localPlayerFeetPosition);
    void shutdown();

    bool isReady() const;

    void setLocalPlayerDesiredVelocity(const util::Vec3& velocity);
    void requestLocalPlayerJump();
    void step(float deltaSeconds);

    util::Vec3 localPlayerFeetPosition() const;
    util::Vec3 localPlayerLinearVelocity() const;
    bool localPlayerSupported() const;
    util::Vec3 localPlayerGroundNormal() const;

    bool castRay(const util::Vec3& from, const util::Vec3& to, float* hitFraction = nullptr) const;

    void spawnThrowable(const std::string& itemId,
                        const util::Vec3& position,
                        const util::Vec3& velocity,
                        float fuseSeconds,
                        float effectRadius);
    std::vector<PhysicsMovementEvent> consumeMovementEvents();
    std::vector<PhysicsDetonationEvent> consumeDetonations();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mycsg::gameplay
