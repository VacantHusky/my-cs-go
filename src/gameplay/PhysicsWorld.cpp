#include "gameplay/PhysicsWorld.h"

#include "renderer/vulkan/MeshRuntime.h"

#include <spdlog/spdlog.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mycsg::gameplay {

namespace {

using namespace JPH;
using namespace JPH::literals;

constexpr float kPlayerRadius = 0.22f;
constexpr float kPlayerBodyHeight = 1.02f;
constexpr float kJumpSpeed = 5.2f;
constexpr float kWorldGravity = 14.0f;
constexpr std::size_t kTempAllocatorBytes = 8 * 1024 * 1024;
constexpr float kGroundAcceleration = 40.0f;
constexpr float kGroundFriction = 11.0f;
constexpr float kGroundStopSpeed = 1.6f;
constexpr float kAirAcceleration = 6.5f;
constexpr float kAirWishSpeedCap = 4.35f;
constexpr float kJumpBufferSeconds = 0.12f;
constexpr float kCoyoteTimeSeconds = 0.10f;
constexpr float kGroundedVerticalTolerance = 0.85f;
constexpr float kStickToFloorDistance = 0.72f;
constexpr float kWalkStairsStepUp = 0.46f;
constexpr float kWalkStairsMinForward = 0.04f;
constexpr float kWalkStairsForwardTest = 0.26f;
constexpr float kWalkStairsStepDownExtra = 0.18f;

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

namespace Layers {
constexpr ObjectLayer NON_MOVING = 0;
constexpr ObjectLayer MOVING = 1;
constexpr ObjectLayer PROJECTILE = 2;
constexpr ObjectLayer NUM_LAYERS = 3;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr BroadPhaseLayer NON_MOVING(0);
constexpr BroadPhaseLayer MOVING(1);
constexpr std::uint32_t NUM_LAYERS = 2;
}  // namespace BroadPhaseLayers

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(const ObjectLayer inObject1, const ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING || inObject2 == Layers::PROJECTILE;
            case Layers::MOVING:
                return true;
            case Layers::PROJECTILE:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

class BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        objectToBroadPhase_[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        objectToBroadPhase_[Layers::MOVING] = BroadPhaseLayers::MOVING;
        objectToBroadPhase_[Layers::PROJECTILE] = BroadPhaseLayers::MOVING;
    }

    std::uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    BroadPhaseLayer GetBroadPhaseLayer(const ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return objectToBroadPhase_[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(const BroadPhaseLayer inLayer) const override {
        switch (static_cast<BroadPhaseLayer::Type>(inLayer)) {
            case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
                return "NON_MOVING";
            case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
                return "MOVING";
            default:
                return "UNKNOWN";
        }
    }
#endif

private:
    BroadPhaseLayer objectToBroadPhase_[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(const ObjectLayer inLayer1, const BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            case Layers::PROJECTILE:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

void bootstrapJolt() {
    static std::once_flag bootstrapOnce;
    std::call_once(bootstrapOnce, [] {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();
        std::atexit([] {
            UnregisterTypes();
            delete Factory::sInstance;
            Factory::sInstance = nullptr;
        });
    });
}

RVec3 toRVec3(const util::Vec3 value) {
    return RVec3(static_cast<Real>(value.x), static_cast<Real>(value.y), static_cast<Real>(value.z));
}

Vec3 toVec3(const util::Vec3 value) {
    return Vec3(value.x, value.y, value.z);
}

util::Vec3 fromRVec3(const RVec3& value) {
    return {
        static_cast<float>(value.GetX()),
        static_cast<float>(value.GetY()),
        static_cast<float>(value.GetZ()),
    };
}

util::Vec3 fromVec3(const Vec3& value) {
    return {
        value.GetX(),
        value.GetY(),
        value.GetZ(),
    };
}

util::Vec3 normalizedPropScale(const MapProp& prop) {
    return {
        std::max(0.001f, std::abs(prop.scale.x)),
        std::max(0.001f, std::abs(prop.scale.y)),
        std::max(0.001f, std::abs(prop.scale.z)),
    };
}

Quat toJoltRotation(const util::Vec3 rotationDegrees) {
    return Quat::sEulerAngles(Vec3(
        DegreesToRadians(rotationDegrees.x),
        DegreesToRadians(rotationDegrees.y),
        DegreesToRadians(rotationDegrees.z)));
}

struct TriangleMeshBuilder {
    VertexList vertices;
    IndexedTriangleList indices;

    void appendTriangle(const util::Vec3 a, const util::Vec3 b, const util::Vec3 c) {
        const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
        vertices.emplace_back(a.x, a.y, a.z);
        vertices.emplace_back(b.x, b.y, b.z);
        vertices.emplace_back(c.x, c.y, c.z);
        IndexedTriangle triangle;
        triangle.mIdx[0] = base;
        triangle.mIdx[1] = base + 1;
        triangle.mIdx[2] = base + 2;
        indices.push_back(triangle);
    }

    void appendQuad(const util::Vec3 a, const util::Vec3 b, const util::Vec3 c, const util::Vec3 d) {
        appendTriangle(a, b, c);
        appendTriangle(a, c, d);
    }
};

void buildMapTriangleMesh(const MapData& map, TriangleMeshBuilder& builder) {
    (void)map;
    (void)builder;
}

float horizontalLength(const util::Vec3& value) {
    return std::sqrt(value.x * value.x + value.z * value.z);
}

float verticalComponent(const util::Vec3& value) {
    return value.y;
}

Vec3 horizontalComponent(const Vec3& value, const Vec3& up) {
    return value - value.Dot(up) * up;
}

Vec3 withVerticalComponent(const Vec3& horizontalValue, const Vec3& up, const float verticalSpeed) {
    return horizontalValue + verticalSpeed * up;
}

Vec3 applyFriction(const Vec3& velocity, const float friction, const float stopSpeed, const float deltaSeconds) {
    const float speed = velocity.Length();
    if (speed <= 0.0001f) {
        return Vec3::sZero();
    }

    const float control = std::max(speed, stopSpeed);
    const float drop = control * friction * deltaSeconds;
    const float nextSpeed = std::max(0.0f, speed - drop);
    if (nextSpeed <= 0.0001f) {
        return Vec3::sZero();
    }

    return velocity * (nextSpeed / speed);
}

Vec3 accelerateTowards(const Vec3& velocity,
                       const Vec3& wishDirection,
                       const float wishSpeed,
                       const float acceleration,
                       const float deltaSeconds) {
    if (wishSpeed <= 0.0001f) {
        return velocity;
    }

    const float currentSpeed = velocity.Dot(wishDirection);
    const float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return velocity;
    }

    const float accelSpeed = std::min(addSpeed, acceleration * wishSpeed * deltaSeconds);
    return velocity + accelSpeed * wishDirection;
}

bool isSupportedCharacter(const CharacterVirtual& character) {
    return character.GetGroundState() == CharacterVirtual::EGroundState::OnGround &&
           !character.IsSlopeTooSteep(character.GetGroundNormal());
}

RefConst<Shape> makeMeshShapeFromCpuMesh(const renderer::vulkan::CpuMesh& cpuMesh) {
    if (!cpuMesh.valid || cpuMesh.vertices.size() < 3) {
        return {};
    }

    VertexList vertices;
    IndexedTriangleList indices;
    vertices.reserve(cpuMesh.vertices.size());
    indices.reserve(cpuMesh.vertices.size() / 3);

    for (const auto& vertex : cpuMesh.vertices) {
        vertices.emplace_back(vertex.px, vertex.py, vertex.pz);
    }
    for (std::uint32_t index = 0; index + 2 < static_cast<std::uint32_t>(cpuMesh.vertices.size()); index += 3) {
        IndexedTriangle triangle;
        triangle.mIdx[0] = index;
        triangle.mIdx[1] = index + 1;
        triangle.mIdx[2] = index + 2;
        indices.push_back(triangle);
    }

    Ref<MeshShapeSettings> meshSettings = new MeshShapeSettings(std::move(vertices), std::move(indices));
    meshSettings->mMaxTrianglesPerLeaf = 4;
    ShapeSettings::ShapeResult result = meshSettings->Create();
    if (result.HasError()) {
        spdlog::warn("[Physics] Failed to create MeshShape from CPU mesh: {}", result.GetError().c_str());
        return {};
    }
    return result.Get();
}

RefConst<Shape> makePropFallbackShape(const MapProp& prop, util::Vec3& outCenterOffset) {
    if (prop.cylindricalFootprint) {
        outCenterOffset = prop.collisionCenterOffset;
        const float halfHeight = std::max(0.05f, prop.collisionHalfExtents.y - 0.06f);
        const float radius = std::max(0.05f, std::max(prop.collisionHalfExtents.x, prop.collisionHalfExtents.z) - 0.06f);
        return new CylinderShape(halfHeight, radius);
    }
    outCenterOffset = prop.collisionCenterOffset;
    return new BoxShape(Vec3(
        std::max(0.05f, prop.collisionHalfExtents.x),
        std::max(0.05f, prop.collisionHalfExtents.y),
        std::max(0.05f, prop.collisionHalfExtents.z)));
}

bool prefersPrimitivePropCollision(const MapProp& prop) {
    return prop.cylindricalFootprint || toLowerAscii(prop.id).find("crate") != std::string::npos;
}

RefConst<Shape> makePropShape(const std::filesystem::path& assetRoot,
                              const MapProp& prop,
                              util::Vec3& outCenterOffset) {
    if (prefersPrimitivePropCollision(prop)) {
        return makePropFallbackShape(prop, outCenterOffset);
    }

    outCenterOffset = {};
    const renderer::vulkan::CpuMesh cpuMesh = renderer::vulkan::loadMeshAsset(assetRoot, prop.modelPath);
    if (RefConst<Shape> meshShape = makeMeshShapeFromCpuMesh(cpuMesh)) {
        return meshShape;
    }

    spdlog::warn("[Physics] Collision mesh cache missing or invalid, falling back for prop {} ({})",
                 prop.id,
                 prop.modelPath.generic_string());
    return makePropFallbackShape(prop, outCenterOffset);
}

}  // namespace

struct PhysicsWorld::Impl {
    struct PropCollisionTemplate {
        RefConst<Shape> shape;
        util::Vec3 centerOffset{};
    };

    struct ProjectileState {
        BodyID bodyId;
        std::string itemId;
        float fuseSeconds = 0.0f;
        float effectRadius = 0.0f;
    };

    BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;
    std::unique_ptr<TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JobSystemSingleThreaded> jobSystem;
    PhysicsSystem physicsSystem;
    RefConst<Shape> playerShape;
    Ref<CharacterVirtual> playerCharacter;
    std::vector<BodyID> worldBodies;
    std::vector<ProjectileState> projectiles;
    std::vector<PhysicsMovementEvent> pendingMovementEvents;
    std::vector<PhysicsDetonationEvent> pendingDetonations;
    util::Vec3 desiredVelocity{};
    util::Vec3 localPlayerGroundNormal{0.0f, 1.0f, 0.0f};
    bool localPlayerSupported = false;
    bool jumpRequested = false;
    float jumpBufferTimer = 0.0f;
    float coyoteTimer = 0.0f;
    bool ready = false;

    bool initialize(const std::filesystem::path& assetRoot,
                    const MapData& map,
                    const util::Vec3& localPlayerFeetPosition) {
        shutdown();
        spdlog::info("[Physics] 初始化开始: props={} spawns={}",
                     map.props.size(),
                     map.spawns.size());
        bootstrapJolt();
        spdlog::info("[Physics] Jolt 类型注册完成。");

        tempAllocator = std::make_unique<TempAllocatorImpl>(static_cast<int>(kTempAllocatorBytes));
        jobSystem = std::make_unique<JobSystemSingleThreaded>();
        jobSystem->Init(cMaxPhysicsJobs);
        spdlog::info("[Physics] 分配器与单线程任务系统已创建。");

        constexpr std::uint32_t kMaxBodies = 8192;
        constexpr std::uint32_t kNumBodyMutexes = 0;
        constexpr std::uint32_t kMaxBodyPairs = 8192;
        constexpr std::uint32_t kMaxContactConstraints = 4096;
        physicsSystem.Init(kMaxBodies,
                           kNumBodyMutexes,
                           kMaxBodyPairs,
                           kMaxContactConstraints,
                           broadPhaseLayerInterface,
                           objectVsBroadPhaseLayerFilter,
                           objectLayerPairFilter);
        physicsSystem.SetGravity(Vec3(0.0f, -kWorldGravity, 0.0f));
        spdlog::info("[Physics] PhysicsSystem 初始化完成。");

        BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

        TriangleMeshBuilder meshBuilder;
        buildMapTriangleMesh(map, meshBuilder);
        spdlog::info("[Physics] 地图碰撞面片已生成: vertices={} triangles={}",
                     meshBuilder.vertices.size(),
                     meshBuilder.indices.size());
        if (!meshBuilder.indices.empty()) {
            Ref<MeshShapeSettings> meshSettings = new MeshShapeSettings(std::move(meshBuilder.vertices), std::move(meshBuilder.indices));
            meshSettings->mMaxTrianglesPerLeaf = 4;
            BodyCreationSettings meshBody(meshSettings, RVec3::sZero(), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
            meshBody.mFriction = 0.95f;
            worldBodies.push_back(bodyInterface.CreateAndAddBody(meshBody, EActivation::DontActivate));
            spdlog::info("[Physics] 地图静态碰撞体已加入世界。");
        }

        std::unordered_map<std::string, PropCollisionTemplate> propShapeCache;
        for (std::size_t propIndex = 0; propIndex < map.props.size(); ++propIndex) {
            const auto& prop = map.props[propIndex];
            spdlog::info("[Physics] 正在处理道具 {}/{}: id={} model={}",
                         propIndex + 1,
                         map.props.size(),
                         prop.id,
                         prop.modelPath.generic_string());
            const std::string cacheKey = prop.modelPath.generic_string();
            util::Vec3 centerOffset{};
            RefConst<Shape> shape;
            if (const auto found = propShapeCache.find(cacheKey); found != propShapeCache.end()) {
                shape = found->second.shape;
                centerOffset = found->second.centerOffset;
                spdlog::info("[Physics] 复用已缓存道具碰撞体: {}", cacheKey);
            } else {
                shape = makePropShape(assetRoot, prop, centerOffset);
                propShapeCache.emplace(cacheKey, PropCollisionTemplate{shape, centerOffset});
            }
            if (shape == nullptr) {
                shape = makePropFallbackShape(prop, centerOffset);
                spdlog::warn("[Physics] 道具改用回退碰撞体: {}", prop.modelPath.generic_string());
            }
            const util::Vec3 scale = normalizedPropScale(prop);
            centerOffset = {
                centerOffset.x * scale.x,
                centerOffset.y * scale.y,
                centerOffset.z * scale.z,
            };
            if (std::abs(scale.x - 1.0f) > 0.001f ||
                std::abs(scale.y - 1.0f) > 0.001f ||
                std::abs(scale.z - 1.0f) > 0.001f) {
                shape = new ScaledShape(shape, toVec3(scale));
            }
            BodyCreationSettings propBody(shape,
                                          toRVec3({prop.position.x + centerOffset.x, prop.position.y + centerOffset.y, prop.position.z + centerOffset.z}),
                                          toJoltRotation(prop.rotationDegrees),
                                          EMotionType::Static,
                                          Layers::NON_MOVING);
            propBody.mFriction = 0.9f;
            worldBodies.push_back(bodyInterface.CreateAndAddBody(propBody, EActivation::DontActivate));
            spdlog::info("[Physics] 道具静态碰撞体已加入世界: {}", prop.id);
        }

        CharacterID::sSetNextCharacterID();
        spdlog::info("[Physics] 正在创建玩家胶囊碰撞体...");
        ShapeSettings::ShapeResult playerShapeResult = RotatedTranslatedShapeSettings(
            Vec3(0.0f, 0.5f * kPlayerBodyHeight + kPlayerRadius, 0.0f),
            Quat::sIdentity(),
            new CapsuleShape(0.5f * kPlayerBodyHeight, kPlayerRadius)).Create();
        if (playerShapeResult.HasError()) {
            spdlog::error("[Physics] 玩家碰撞体创建失败: {}", playerShapeResult.GetError().c_str());
            shutdown();
            return false;
        }
        playerShape = playerShapeResult.Get();

        Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
        settings->mMaxSlopeAngle = DegreesToRadians(55.0f);
        settings->mMaxStrength = 2000.0f;
        settings->mShape = playerShape;
        settings->mCharacterPadding = 0.015f;
        settings->mPenetrationRecoverySpeed = 0.9f;
        settings->mPredictiveContactDistance = 0.12f;
        settings->mSupportingVolume = Plane(Vec3::sAxisY(), -kPlayerRadius);
        settings->mEnhancedInternalEdgeRemoval = true;
        spdlog::info("[Physics] 玩家 CharacterVirtual 设置已完成。");

        playerCharacter = new CharacterVirtual(settings, toRVec3(localPlayerFeetPosition), Quat::sIdentity(), 0, &physicsSystem);
        spdlog::info("[Physics] CharacterVirtual 已创建，位置=({}, {}, {})",
                     localPlayerFeetPosition.x,
                     localPlayerFeetPosition.y,
                     localPlayerFeetPosition.z);
        playerCharacter->RefreshContacts(physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                                         physicsSystem.GetDefaultLayerFilter(Layers::MOVING),
                                         {},
                                         {},
                                         *tempAllocator);
        spdlog::info("[Physics] 玩家初始接触面刷新完成。");

        desiredVelocity = {};
        localPlayerSupported = isSupportedCharacter(*playerCharacter);
        if (localPlayerSupported) {
            localPlayerGroundNormal = fromVec3(playerCharacter->GetGroundNormal());
        } else {
            localPlayerGroundNormal = {0.0f, 1.0f, 0.0f};
        }
        jumpRequested = false;
        jumpBufferTimer = 0.0f;
        coyoteTimer = localPlayerSupported ? kCoyoteTimeSeconds : 0.0f;
        ready = true;
        spdlog::info("[Physics] Jolt world ready. Static bodies={}, props={}, player initialized.",
                     worldBodies.size(),
                     map.props.size());
        return true;
    }

    void shutdown() {
        if (!ready && !tempAllocator && !jobSystem) {
            return;
        }

        if (tempAllocator && jobSystem) {
            BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
            for (const auto& projectile : projectiles) {
                if (bodyInterface.IsAdded(projectile.bodyId)) {
                    bodyInterface.RemoveBody(projectile.bodyId);
                }
                bodyInterface.DestroyBody(projectile.bodyId);
            }
            for (const BodyID bodyId : worldBodies) {
                if (bodyInterface.IsAdded(bodyId)) {
                    bodyInterface.RemoveBody(bodyId);
                }
                bodyInterface.DestroyBody(bodyId);
            }
        }

        projectiles.clear();
        worldBodies.clear();
        playerCharacter = nullptr;
        playerShape = nullptr;
        pendingMovementEvents.clear();
        pendingDetonations.clear();
        tempAllocator.reset();
        jobSystem.reset();
        desiredVelocity = {};
        localPlayerGroundNormal = {0.0f, 1.0f, 0.0f};
        localPlayerSupported = false;
        jumpRequested = false;
        jumpBufferTimer = 0.0f;
        coyoteTimer = 0.0f;
        ready = false;
    }

    void step(const float deltaSeconds) {
        if (!ready || playerCharacter == nullptr || deltaSeconds <= 0.0f) {
            return;
        }

        const bool wasSupported = isSupportedCharacter(*playerCharacter);
        const util::Vec3 previousFeetPosition = fromRVec3(playerCharacter->GetPosition());
        const util::Vec3 previousLinearVelocity = fromVec3(playerCharacter->GetLinearVelocity());

        playerCharacter->UpdateGroundVelocity();

        const Vec3 up = playerCharacter->GetUp();
        const Vec3 currentVelocity = playerCharacter->GetLinearVelocity();
        const Vec3 currentHorizontalVelocity = horizontalComponent(currentVelocity, up);
        const Vec3 groundVelocity = playerCharacter->GetGroundVelocity();
        const Vec3 groundHorizontalVelocity = horizontalComponent(groundVelocity, up);
        const float currentVerticalSpeed = currentVelocity.Dot(up);
        const float groundVerticalSpeed = groundVelocity.Dot(up);
        const bool movingTowardsGround = currentVerticalSpeed - groundVerticalSpeed <= kGroundedVerticalTolerance;
        const bool supported = wasSupported && movingTowardsGround;
        const Vec3 wishHorizontalVelocity = toVec3(desiredVelocity);
        const float wishHorizontalSpeed = wishHorizontalVelocity.Length();
        const Vec3 wishDirection = wishHorizontalSpeed > 0.0001f
            ? wishHorizontalVelocity / wishHorizontalSpeed
            : Vec3::sZero();
        Vec3 newHorizontalVelocity = currentHorizontalVelocity;

        jumpBufferTimer = std::max(0.0f, jumpBufferTimer - deltaSeconds);
        if (jumpRequested) {
            jumpBufferTimer = kJumpBufferSeconds;
        }
        coyoteTimer = supported ? kCoyoteTimeSeconds : std::max(0.0f, coyoteTimer - deltaSeconds);

        if (supported) {
            Vec3 relativeHorizontalVelocity = currentHorizontalVelocity - groundHorizontalVelocity;
            relativeHorizontalVelocity = applyFriction(relativeHorizontalVelocity,
                                                      kGroundFriction,
                                                      kGroundStopSpeed,
                                                      deltaSeconds);
            if (wishHorizontalSpeed > 0.0001f) {
                relativeHorizontalVelocity = accelerateTowards(relativeHorizontalVelocity,
                                                               wishDirection,
                                                               wishHorizontalSpeed,
                                                               kGroundAcceleration,
                                                               deltaSeconds);
            }
            newHorizontalVelocity = groundHorizontalVelocity + relativeHorizontalVelocity;
        } else {
            newHorizontalVelocity = currentHorizontalVelocity;
            if (wishHorizontalSpeed > 0.0001f) {
                newHorizontalVelocity = accelerateTowards(newHorizontalVelocity,
                                                          wishDirection,
                                                          std::min(wishHorizontalSpeed, kAirWishSpeedCap),
                                                          kAirAcceleration,
                                                          deltaSeconds);
            }
        }

        bool performedJump = false;
        Vec3 newVelocity = withVerticalComponent(newHorizontalVelocity, up, currentVerticalSpeed);
        if (jumpBufferTimer > 0.0f && coyoteTimer > 0.0f) {
            const Vec3 jumpBaseHorizontal = supported ? newHorizontalVelocity : currentHorizontalVelocity;
            newVelocity = jumpBaseHorizontal + (kJumpSpeed * up);
            pendingMovementEvents.push_back({
                PhysicsMovementEventType::Jumped,
                1.0f,
                0.0f,
            });
            jumpBufferTimer = 0.0f;
            coyoteTimer = 0.0f;
            performedJump = true;
        } else if (!supported) {
            newVelocity += physicsSystem.GetGravity() * deltaSeconds;
        }

        if (performedJump) {
            newVelocity += physicsSystem.GetGravity() * deltaSeconds * 0.35f;
        }

        playerCharacter->SetLinearVelocity(newVelocity);

        CharacterVirtual::ExtendedUpdateSettings updateSettings;
        updateSettings.mStickToFloorStepDown = performedJump ? Vec3::sZero() : (-up * kStickToFloorDistance);
        updateSettings.mWalkStairsStepUp = up * kWalkStairsStepUp;
        updateSettings.mWalkStairsMinStepForward = kWalkStairsMinForward;
        updateSettings.mWalkStairsStepForwardTest = kWalkStairsForwardTest;
        updateSettings.mWalkStairsStepDownExtra = -up * kWalkStairsStepDownExtra;
        playerCharacter->ExtendedUpdate(deltaSeconds,
                                        -up * physicsSystem.GetGravity().Length(),
                                        updateSettings,
                                        physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                                        physicsSystem.GetDefaultLayerFilter(Layers::MOVING),
                                        {},
                                        {},
                                        *tempAllocator);

        const util::Vec3 currentFeetPosition = fromRVec3(playerCharacter->GetPosition());
        localPlayerSupported = isSupportedCharacter(*playerCharacter);
        localPlayerGroundNormal = localPlayerSupported
            ? fromVec3(playerCharacter->GetGroundNormal())
            : util::Vec3{0.0f, 1.0f, 0.0f};

        const float heightDelta = currentFeetPosition.y - previousFeetPosition.y;
        const float movedHorizontally = horizontalLength({
            currentFeetPosition.x - previousFeetPosition.x,
            0.0f,
            currentFeetPosition.z - previousFeetPosition.z,
        });
        if (!wasSupported && localPlayerSupported) {
            const float landingSpeed = std::max(0.0f, -verticalComponent(previousLinearVelocity));
            pendingMovementEvents.push_back({
                PhysicsMovementEventType::Landed,
                std::clamp(landingSpeed / 6.0f, 0.0f, 1.0f),
                0.0f,
            });
        } else if (wasSupported && localPlayerSupported && heightDelta > 0.05f && heightDelta < 0.48f && movedHorizontally > 0.04f) {
            pendingMovementEvents.push_back({
                PhysicsMovementEventType::SteppedUp,
                std::clamp(heightDelta / 0.45f, 0.0f, 1.0f),
                heightDelta,
            });
        }

        const int collisionSteps = std::max(1, static_cast<int>(std::ceil(deltaSeconds * 60.0f)));
        physicsSystem.Update(deltaSeconds, collisionSteps, tempAllocator.get(), jobSystem.get());
        jumpRequested = false;

        BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
        for (auto it = projectiles.begin(); it != projectiles.end();) {
            it->fuseSeconds -= deltaSeconds;
            if (it->fuseSeconds > 0.0f) {
                ++it;
                continue;
            }

            const util::Vec3 detonationPosition = fromRVec3(bodyInterface.GetCenterOfMassPosition(it->bodyId));
            if (bodyInterface.IsAdded(it->bodyId)) {
                bodyInterface.RemoveBody(it->bodyId);
            }
            bodyInterface.DestroyBody(it->bodyId);
            pendingDetonations.push_back({it->itemId, detonationPosition, it->effectRadius});
            it = projectiles.erase(it);
        }
    }

    bool castRay(const util::Vec3& from, const util::Vec3& to, float* hitFraction) const {
        if (!ready) {
            return false;
        }

        const util::Vec3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
        const float lengthSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (lengthSquared <= 0.000001f) {
            return false;
        }

        RRayCast ray(toRVec3(from), toVec3(delta));
        RayCastResult hit;
        const bool hadHit = physicsSystem.GetNarrowPhaseQuery().CastRay(
            ray,
            hit,
            SpecifiedBroadPhaseLayerFilter(BroadPhaseLayers::NON_MOVING),
            SpecifiedObjectLayerFilter(Layers::NON_MOVING));
        if (hadHit && hitFraction != nullptr) {
            *hitFraction = hit.mFraction;
        }
        return hadHit;
    }

    util::Vec3 localPlayerFeetPosition() const {
        if (!ready || playerCharacter == nullptr) {
            return {};
        }
        return fromRVec3(playerCharacter->GetPosition());
    }

    util::Vec3 localPlayerLinearVelocity() const {
        if (!ready || playerCharacter == nullptr) {
            return {};
        }
        return fromVec3(playerCharacter->GetLinearVelocity());
    }

    bool localPlayerSupportedNow() const {
        return ready && playerCharacter != nullptr && localPlayerSupported;
    }

    util::Vec3 currentGroundNormal() const {
        if (!ready || playerCharacter == nullptr) {
            return {0.0f, 1.0f, 0.0f};
        }
        return localPlayerGroundNormal;
    }

    void spawnThrowable(const std::string& itemId,
                        const util::Vec3& position,
                        const util::Vec3& velocity,
                        const float fuseSeconds,
                        const float effectRadius) {
        if (!ready) {
            return;
        }

        BodyCreationSettings bodySettings(new SphereShape(0.08f),
                                          toRVec3(position),
                                          Quat::sIdentity(),
                                          EMotionType::Dynamic,
                                          Layers::PROJECTILE);
        bodySettings.mMotionQuality = EMotionQuality::LinearCast;
        bodySettings.mRestitution = 0.28f;
        bodySettings.mFriction = 0.65f;
        bodySettings.mLinearDamping = 0.12f;
        bodySettings.mAngularDamping = 0.20f;
        bodySettings.mGravityFactor = 1.0f;
        bodySettings.mLinearVelocity = toVec3(velocity);

        BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
        const BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, EActivation::Activate);
        projectiles.push_back({bodyId, itemId, fuseSeconds, effectRadius});
    }

    std::vector<PhysicsDetonationEvent> consumeDetonations() {
        std::vector<PhysicsDetonationEvent> out = std::move(pendingDetonations);
        pendingDetonations.clear();
        return out;
    }

    std::vector<PhysicsMovementEvent> consumeMovementEvents() {
        std::vector<PhysicsMovementEvent> out = std::move(pendingMovementEvents);
        pendingMovementEvents.clear();
        return out;
    }
};

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {}
PhysicsWorld::~PhysicsWorld() = default;

bool PhysicsWorld::initialize(const std::filesystem::path& assetRoot,
                              const MapData& map,
                              const util::Vec3& localPlayerFeetPosition) {
    return impl_->initialize(assetRoot, map, localPlayerFeetPosition);
}

void PhysicsWorld::shutdown() {
    impl_->shutdown();
}

bool PhysicsWorld::isReady() const {
    return impl_->ready;
}

void PhysicsWorld::setLocalPlayerDesiredVelocity(const util::Vec3& velocity) {
    impl_->desiredVelocity = velocity;
}

void PhysicsWorld::requestLocalPlayerJump() {
    impl_->jumpRequested = true;
}

void PhysicsWorld::step(const float deltaSeconds) {
    impl_->step(deltaSeconds);
}

util::Vec3 PhysicsWorld::localPlayerFeetPosition() const {
    return impl_->localPlayerFeetPosition();
}

util::Vec3 PhysicsWorld::localPlayerLinearVelocity() const {
    return impl_->localPlayerLinearVelocity();
}

bool PhysicsWorld::localPlayerSupported() const {
    return impl_->localPlayerSupportedNow();
}

util::Vec3 PhysicsWorld::localPlayerGroundNormal() const {
    return impl_->currentGroundNormal();
}

bool PhysicsWorld::castRay(const util::Vec3& from, const util::Vec3& to, float* hitFraction) const {
    return impl_->castRay(from, to, hitFraction);
}

void PhysicsWorld::spawnThrowable(const std::string& itemId,
                                  const util::Vec3& position,
                                  const util::Vec3& velocity,
                                  const float fuseSeconds,
                                  const float effectRadius) {
    impl_->spawnThrowable(itemId, position, velocity, fuseSeconds, effectRadius);
}

std::vector<PhysicsMovementEvent> PhysicsWorld::consumeMovementEvents() {
    return impl_->consumeMovementEvents();
}

std::vector<PhysicsDetonationEvent> PhysicsWorld::consumeDetonations() {
    return impl_->consumeDetonations();
}

}  // namespace mycsg::gameplay
