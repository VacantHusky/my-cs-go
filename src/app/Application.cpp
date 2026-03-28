#include "app/Application.h"

#include "platform/Window.h"
#include "renderer/Renderer.h"
#include "util/FileSystem.h"
#include "util/Log.h"
#include "util/MapEditorCameraMath.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <set>
#include <thread>

namespace mycsg::app {

namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kSinglePlayerEyeHeight = 1.0f;
constexpr float kMapEditorDefaultPlacementDistance = 7.5f;
constexpr float kMapEditorMaxPlacementDistance = 128.0f;
constexpr std::size_t kMapEditorUndoLimit = 96;

struct RaySurfaceHit {
    util::Vec3 point{};
    util::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = std::numeric_limits<float>::max();
};

util::Vec3 addVec3(const util::Vec3& lhs, const util::Vec3& rhs);
util::Vec3 subtractVec3(const util::Vec3& lhs, const util::Vec3& rhs);
util::Vec3 multiplyVec3(const util::Vec3& value, float scalar);
float dotVec3(const util::Vec3& lhs, const util::Vec3& rhs);
float lengthSquaredVec3(const util::Vec3& value);
float lengthVec3(const util::Vec3& value);
util::Vec3 normalizeVec3(const util::Vec3& value);

std::filesystem::path assetRootPath() {
#ifdef _WIN32
    return "assets";
#elif defined(MYCSGO_ASSET_ROOT)
    return MYCSGO_ASSET_ROOT;
#else
    return "assets";
#endif
}

float degreesToRadians(const float degrees) {
    return degrees * (kPi / 180.0f);
}

float approachZero(const float value, const float amount) {
    if (value > 0.0f) {
        return std::max(0.0f, value - amount);
    }
    if (value < 0.0f) {
        return std::min(0.0f, value + amount);
    }
    return 0.0f;
}

float randomSigned(std::minstd_rand& generator) {
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    return distribution(generator);
}

float trainingBaseSpread(const content::WeaponDefinition& weapon) {
    return std::max(0.18f, weapon.aimSpread * 0.72f);
}

constexpr std::size_t kSettingsEntryCount = 4;
constexpr std::size_t kMultiplayerEntryCount = 4;

struct EditorPropScalePreset {
    float uniformScale;
    const char* label;
};

constexpr std::array<EditorPropScalePreset, 5> kEditorPropScalePresets{{
    {0.75f, "0.75x"},
    {1.0f, "1.00x"},
    {1.25f, "1.25x"},
    {1.5f, "1.50x"},
    {2.0f, "2.00x"},
}};

const char* settingToggleLabel(const bool enabled) {
    return enabled ? "开启" : "关闭";
}

const EditorPropScalePreset& editorPropScalePreset(const std::size_t index) {
    return kEditorPropScalePresets[std::min(index, kEditorPropScalePresets.size() - 1)];
}

const char* sessionTypeLabel(const network::SessionType type) {
    switch (type) {
        case network::SessionType::Offline:
            return "离线";
        case network::SessionType::Host:
            return "主机";
        case network::SessionType::Client:
            return "客户端";
    }
    return "主机";
}

struct WeaponHandlingProfile {
    float shotSpreadKick = 0.6f;
    float moveSpreadGain = 2.4f;
    float spreadRecovery = 4.5f;
    float recoilReturnSpeed = 10.0f;
    float yawKickScale = 1.0f;
    float viewKickScale = 1.0f;
    float maxSpread = 5.0f;
    float targetRadius = 0.30f;
    float effectiveRange = 28.0f;
    float muzzleFlashDuration = 0.05f;
};

WeaponHandlingProfile makeHandlingProfile(const content::WeaponDefinition& weapon) {
    WeaponHandlingProfile profile{};
    profile.maxSpread = std::max(trainingBaseSpread(weapon) + 1.4f, weapon.hipSpread * 1.9f);

    switch (weapon.category) {
        case content::WeaponCategory::Rifle:
            profile.shotSpreadKick = 0.34f + weapon.hipSpread * 0.14f;
            profile.moveSpreadGain = 2.1f;
            profile.spreadRecovery = 4.8f;
            profile.recoilReturnSpeed = 10.5f;
            profile.yawKickScale = 1.0f;
            profile.viewKickScale = 0.95f;
            profile.targetRadius = 0.28f;
            profile.effectiveRange = 34.0f;
            profile.muzzleFlashDuration = 0.05f;
            break;
        case content::WeaponCategory::SniperRifle:
            profile.shotSpreadKick = 0.55f + weapon.hipSpread * 0.10f;
            profile.moveSpreadGain = 4.2f;
            profile.spreadRecovery = 5.8f;
            profile.recoilReturnSpeed = 8.2f;
            profile.yawKickScale = 0.72f;
            profile.viewKickScale = 1.45f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 3.4f, weapon.hipSpread * 1.35f);
            profile.targetRadius = 0.18f;
            profile.effectiveRange = 48.0f;
            profile.muzzleFlashDuration = 0.08f;
            break;
        case content::WeaponCategory::SubmachineGun:
            profile.shotSpreadKick = 0.26f + weapon.hipSpread * 0.18f;
            profile.moveSpreadGain = 3.0f;
            profile.spreadRecovery = 5.4f;
            profile.recoilReturnSpeed = 11.2f;
            profile.yawKickScale = 0.9f;
            profile.viewKickScale = 0.70f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 2.2f, weapon.hipSpread * 2.25f);
            profile.targetRadius = 0.33f;
            profile.effectiveRange = 24.0f;
            profile.muzzleFlashDuration = 0.045f;
            break;
        case content::WeaponCategory::Shotgun:
            profile.shotSpreadKick = 0.82f + weapon.hipSpread * 0.12f;
            profile.moveSpreadGain = 3.8f;
            profile.spreadRecovery = 4.1f;
            profile.recoilReturnSpeed = 8.6f;
            profile.yawKickScale = 1.2f;
            profile.viewKickScale = 1.25f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 2.8f, weapon.hipSpread * 1.75f);
            profile.targetRadius = 0.72f;
            profile.effectiveRange = 14.0f;
            profile.muzzleFlashDuration = 0.07f;
            break;
        case content::WeaponCategory::Melee:
        case content::WeaponCategory::Grenade:
            break;
    }

    return profile;
}

const char* opticLabel(const content::OpticType optic) {
    switch (optic) {
        case content::OpticType::IronSight:
            return "机瞄";
        case content::OpticType::RedDot:
            return "红点";
        case content::OpticType::X2:
            return "2倍";
        case content::OpticType::X4:
            return "4倍";
        case content::OpticType::X8:
            return "8倍";
    }
    return "红点";
}

float opticMagnification(const content::OpticType optic) {
    switch (optic) {
        case content::OpticType::IronSight:
        case content::OpticType::RedDot:
            return 1.0f;
        case content::OpticType::X2:
            return 2.0f;
        case content::OpticType::X4:
            return 4.0f;
        case content::OpticType::X8:
            return 8.0f;
    }
    return 1.0f;
}

std::string throwableLabel(const std::size_t index) {
    switch (index % 3) {
        case 0:
            return "破片手雷";
        case 1:
            return "闪光弹";
        default:
            return "烟雾弹";
    }
}

std::string trimAscii(std::string value) {
    const auto isSpace = [](const unsigned char character) {
        return std::isspace(character) != 0;
    };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string sanitizePlayerToken(const std::string& value) {
    std::string token;
    token.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            token.push_back(static_cast<char>(std::tolower(character)));
        } else if (character == '_' || character == '-') {
            token.push_back(static_cast<char>(character));
        }
    }
    if (token.empty()) {
        token = "player";
    }
    return token;
}

std::string makeSessionLocalPlayerId(const network::SessionType type, const std::string& playerName) {
    if (type != network::SessionType::Client) {
        return "p0";
    }

    const auto tickCount = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream id;
    id << "client_" << sanitizePlayerToken(playerName) << '_'
       << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned int>(tickCount & 0xffffffffu);
    return id.str();
}

std::string propDisplayLabel(const gameplay::MapProp& prop) {
    if (!prop.label.empty()) {
        return prop.label;
    }
    return prop.id.empty() ? "道具" : prop.id;
}

util::Vec3 editorPropHalfExtents(const gameplay::MapProp& prop);

util::Vec3 centerOfCell(const int cellX, const int cellZ, const float y = 0.0f) {
    return {static_cast<float>(cellX) + 0.5f, y, static_cast<float>(cellZ) + 0.5f};
}

bool positionInsideCell(const util::Vec3& position, const int cellX, const int cellZ) {
    return static_cast<int>(std::floor(position.x)) == cellX &&
           static_cast<int>(std::floor(position.z)) == cellZ;
}

bool pointHitsPropFootprint(const gameplay::MapProp& prop, const float x, const float z) {
    const util::Vec3 halfExtents = editorPropHalfExtents(prop);
    const float dx = x - prop.position.x;
    const float dz = z - prop.position.z;
    if (prop.cylindricalFootprint) {
        const float radius = std::max(halfExtents.x, halfExtents.z);
        return dx * dx + dz * dz <= radius * radius;
    }
    return std::abs(dx) <= halfExtents.x && std::abs(dz) <= halfExtents.z;
}

std::string pathDisplayLabel(const std::filesystem::path& path) {
    if (path.empty()) {
        return "无";
    }
    return path.generic_string();
}

float wrapDegrees(const float degrees) {
    return std::remainder(degrees, 360.0f);
}

util::Vec3 sanitizeEditorPropScale(const util::Vec3& scale) {
    return {
        std::max(0.05f, std::abs(scale.x)),
        std::max(0.05f, std::abs(scale.y)),
        std::max(0.05f, std::abs(scale.z)),
    };
}

util::Vec3 editorPropHalfExtents(const gameplay::MapProp& prop) {
    const util::Vec3 scale = sanitizeEditorPropScale(prop.scale);
    return {
        std::max(0.01f, prop.collisionHalfExtents.x) * scale.x,
        std::max(0.01f, prop.collisionHalfExtents.y) * scale.y,
        std::max(0.01f, prop.collisionHalfExtents.z) * scale.z,
    };
}

float componentAt(const util::Vec3& value, const int axis) {
    switch (axis) {
        case 0: return value.x;
        case 1: return value.y;
        default: return value.z;
    }
}

void setAxis(util::Vec3& value, const int axis, const float component) {
    switch (axis) {
        case 0:
            value.x = component;
            break;
        case 1:
            value.y = component;
            break;
        default:
            value.z = component;
            break;
    }
}

util::Vec3 rotateAroundX(const util::Vec3& value, const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x,
        value.y * cosine - value.z * sine,
        value.y * sine + value.z * cosine,
    };
}

util::Vec3 rotateAroundY(const util::Vec3& value, const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine + value.z * sine,
        value.y,
        -value.x * sine + value.z * cosine,
    };
}

util::Vec3 rotateAroundZ(const util::Vec3& value, const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine,
        value.z,
    };
}

util::Vec3 rotateEditorVector(const util::Vec3& value, const util::Vec3& rotationDegrees) {
    util::Vec3 rotated = rotateAroundZ(value, degreesToRadians(rotationDegrees.z));
    rotated = rotateAroundX(rotated, degreesToRadians(rotationDegrees.x));
    rotated = rotateAroundY(rotated, degreesToRadians(rotationDegrees.y));
    return rotated;
}

util::Vec3 inverseRotateEditorVector(const util::Vec3& value, const util::Vec3& rotationDegrees) {
    util::Vec3 rotated = rotateAroundY(value, -degreesToRadians(rotationDegrees.y));
    rotated = rotateAroundX(rotated, -degreesToRadians(rotationDegrees.x));
    rotated = rotateAroundZ(rotated, -degreesToRadians(rotationDegrees.z));
    return rotated;
}

util::Vec3 editorPropWorldCenter(const gameplay::MapProp& prop) {
    const util::Vec3 halfExtents = editorPropHalfExtents(prop);
    return addVec3(prop.position, rotateEditorVector({0.0f, halfExtents.y, 0.0f}, prop.rotationDegrees));
}

bool pointInsidePropVolume(const gameplay::MapProp& prop, const util::Vec3& position) {
    const util::Vec3 halfExtents = editorPropHalfExtents(prop);
    const util::Vec3 center = editorPropWorldCenter(prop);
    const util::Vec3 localPoint = inverseRotateEditorVector(subtractVec3(position, center), prop.rotationDegrees);
    return std::abs(localPoint.x) <= halfExtents.x &&
        std::abs(localPoint.y) <= halfExtents.y &&
        std::abs(localPoint.z) <= halfExtents.z;
}

util::Vec3 clampEditorTargetPosition(const gameplay::MapData& map, const util::Vec3& position) {
    const float maxX = std::max(0.0f, static_cast<float>(std::max(map.width, 1)) - 0.001f);
    const float maxZ = std::max(0.0f, static_cast<float>(std::max(map.depth, 1)) - 0.001f);
    const float yExtent = std::max(12.0f, static_cast<float>(std::max(map.height, 1)) * 2.0f);
    return {
        std::clamp(position.x, 0.0f, maxX),
        std::clamp(position.y, -yExtent, yExtent),
        std::clamp(position.z, 0.0f, maxZ),
    };
}

util::Vec3 defaultFloatingEditorTargetPosition(const gameplay::MapData& map,
                                               const util::Vec3& origin,
                                               const util::Vec3& direction) {
    return clampEditorTargetPosition(map, addVec3(origin, multiplyVec3(direction, kMapEditorDefaultPlacementDistance)));
}

bool rayIntersectAabb(const util::Vec3& origin,
                      const util::Vec3& direction,
                      const util::Vec3& minimum,
                      const util::Vec3& maximum,
                      const float maxDistance,
                      float& outDistance,
                      util::Vec3& outPoint,
                      util::Vec3& outNormal) {
    constexpr float kEpsilon = 0.00001f;
    float entryDistance = 0.0f;
    float exitDistance = maxDistance;
    util::Vec3 entryNormal{};
    util::Vec3 exitNormal{};

    for (int axis = 0; axis < 3; ++axis) {
        const float originComponent = componentAt(origin, axis);
        const float directionComponent = componentAt(direction, axis);
        const float minComponent = componentAt(minimum, axis);
        const float maxComponent = componentAt(maximum, axis);

        if (std::abs(directionComponent) <= kEpsilon) {
            if (originComponent < minComponent || originComponent > maxComponent) {
                return false;
            }
            continue;
        }

        float nearDistance = (minComponent - originComponent) / directionComponent;
        float farDistance = (maxComponent - originComponent) / directionComponent;
        util::Vec3 nearNormal{};
        util::Vec3 farNormal{};
        setAxis(nearNormal, axis, -1.0f);
        setAxis(farNormal, axis, 1.0f);
        if (nearDistance > farDistance) {
            std::swap(nearDistance, farDistance);
            std::swap(nearNormal, farNormal);
        }

        if (nearDistance > entryDistance) {
            entryDistance = nearDistance;
            entryNormal = nearNormal;
        }
        if (farDistance < exitDistance) {
            exitDistance = farDistance;
            exitNormal = farNormal;
        }
        if (entryDistance > exitDistance) {
            return false;
        }
    }

    if (exitDistance < 0.0f) {
        return false;
    }

    const bool startedInside = entryDistance <= kEpsilon;
    outDistance = startedInside ? exitDistance : entryDistance;
    if (outDistance < 0.0f || outDistance > maxDistance) {
        return false;
    }
    outNormal = startedInside ? exitNormal : entryNormal;
    outPoint = addVec3(origin, multiplyVec3(direction, outDistance));
    return true;
}

bool rayIntersectEditorProp(const gameplay::MapProp& prop,
                            const util::Vec3& origin,
                            const util::Vec3& direction,
                            const float maxDistance,
                            RaySurfaceHit& outHit) {
    const util::Vec3 halfExtents = editorPropHalfExtents(prop);
    const util::Vec3 center = editorPropWorldCenter(prop);
    const util::Vec3 localOrigin = inverseRotateEditorVector(subtractVec3(origin, center), prop.rotationDegrees);
    const util::Vec3 localDirection = inverseRotateEditorVector(direction, prop.rotationDegrees);

    float distance = 0.0f;
    util::Vec3 localPoint{};
    util::Vec3 localNormal{};
    if (!rayIntersectAabb(localOrigin, localDirection, multiplyVec3(halfExtents, -1.0f), halfExtents,
            maxDistance, distance, localPoint, localNormal)) {
        return false;
    }

    outHit.point = addVec3(center, rotateEditorVector(localPoint, prop.rotationDegrees));
    outHit.normal = normalizeVec3(rotateEditorVector(localNormal, prop.rotationDegrees));
    if (lengthSquaredVec3(outHit.normal) <= 0.0001f) {
        outHit.normal = {0.0f, 1.0f, 0.0f};
    }
    outHit.distance = distance;
    return true;
}

bool rayIntersectGroundPlane(const gameplay::MapData& map,
                             const util::Vec3& origin,
                             const util::Vec3& direction,
                             const float maxDistance,
                             RaySurfaceHit& outHit) {
    if (std::abs(direction.y) <= 0.0001f) {
        return false;
    }

    const float distance = -origin.y / direction.y;
    if (distance <= 0.0f || distance > maxDistance) {
        return false;
    }

    const util::Vec3 point = addVec3(origin, multiplyVec3(direction, distance));
    if (point.x < 0.0f || point.z < 0.0f ||
        point.x >= static_cast<float>(map.width) ||
        point.z >= static_cast<float>(map.depth)) {
        return false;
    }

    outHit.point = point;
    outHit.normal = {0.0f, 1.0f, 0.0f};
    outHit.distance = distance;
    return true;
}

std::optional<std::size_t> pickMapEditorPropFromRay(const gameplay::MapData& map,
                                                    const util::Vec3& origin,
                                                    const util::Vec3& direction,
                                                    const float maxDistance,
                                                    float* outDistance = nullptr,
                                                    RaySurfaceHit* outHit = nullptr) {
    std::optional<std::size_t> bestIndex;
    float bestDistance = std::numeric_limits<float>::max();
    RaySurfaceHit bestHit{};

    for (std::size_t index = 0; index < map.props.size(); ++index) {
        RaySurfaceHit hit;
        if (!rayIntersectEditorProp(map.props[index], origin, direction, maxDistance, hit)) {
            continue;
        }
        if (!bestIndex.has_value() || hit.distance < bestDistance) {
            bestIndex = index;
            bestDistance = hit.distance;
            bestHit = hit;
        }
    }

    if (outDistance != nullptr) {
        *outDistance = bestIndex.has_value() ? bestDistance : std::numeric_limits<float>::max();
    }
    if (outHit != nullptr && bestIndex.has_value()) {
        *outHit = bestHit;
    }
    return bestIndex;
}

std::optional<std::size_t> pickMapEditorSpawnFromRay(const gameplay::MapData& map,
                                                     const util::Vec3& origin,
                                                     const util::Vec3& direction,
                                                     const float maxDistance,
                                                     const float selectionRadius,
                                                     float* outDistance = nullptr) {
    std::optional<std::size_t> bestIndex;
    float bestDistance = std::numeric_limits<float>::max();

    for (std::size_t index = 0; index < map.spawns.size(); ++index) {
        const util::Vec3 center = map.spawns[index].position;
        const util::Vec3 toCenter = subtractVec3(center, origin);
        const float projection = dotVec3(toCenter, direction);
        if (projection < 0.0f || projection > maxDistance) {
            continue;
        }

        const util::Vec3 closestPoint = addVec3(origin, multiplyVec3(direction, projection));
        if (lengthSquaredVec3(subtractVec3(center, closestPoint)) > selectionRadius * selectionRadius) {
            continue;
        }

        if (!bestIndex.has_value() || projection < bestDistance) {
            bestIndex = index;
            bestDistance = projection;
        }
    }

    if (outDistance != nullptr) {
        *outDistance = bestIndex.has_value() ? bestDistance : std::numeric_limits<float>::max();
    }
    return bestIndex;
}

util::Vec3 editorPlacementOriginFromTarget(const gameplay::MapProp& prop,
                                           const util::Vec3& targetPoint,
                                           const util::Vec3& targetNormal,
                                           const bool targetOnSurface) {
    if (!targetOnSurface) {
        return targetPoint;
    }

    const util::Vec3 normal = normalizeVec3(targetNormal);
    if (lengthSquaredVec3(normal) <= 0.0001f) {
        return targetPoint;
    }

    const util::Vec3 halfExtents = editorPropHalfExtents(prop);
    const util::Vec3 axisX = normalizeVec3(rotateEditorVector({1.0f, 0.0f, 0.0f}, prop.rotationDegrees));
    const util::Vec3 axisY = normalizeVec3(rotateEditorVector({0.0f, 1.0f, 0.0f}, prop.rotationDegrees));
    const util::Vec3 axisZ = normalizeVec3(rotateEditorVector({0.0f, 0.0f, 1.0f}, prop.rotationDegrees));
    const float supportExtent =
        std::abs(dotVec3(normal, axisX)) * halfExtents.x +
        std::abs(dotVec3(normal, axisY)) * halfExtents.y +
        std::abs(dotVec3(normal, axisZ)) * halfExtents.z;
    const util::Vec3 centerOffset = rotateEditorVector({0.0f, halfExtents.y, 0.0f}, prop.rotationDegrees);
    const util::Vec3 centeredPosition = addVec3(targetPoint, multiplyVec3(normal, supportExtent + 0.01f));
    return subtractVec3(centeredPosition, centerOffset);
}

float spawnSelectionRadius() {
    return 0.60f;
}

util::Vec3 addVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

util::Vec3 subtractVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

util::Vec3 multiplyVec3(const util::Vec3& value, const float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float dotVec3(const util::Vec3& lhs, const util::Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float lengthSquaredVec3(const util::Vec3& value) {
    return dotVec3(value, value);
}

float lengthVec3(const util::Vec3& value) {
    return std::sqrt(lengthSquaredVec3(value));
}

util::Vec3 normalizeVec3(const util::Vec3& value) {
    const float length = lengthVec3(value);
    if (length <= 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return multiplyVec3(value, 1.0f / length);
}

renderer::RenderFrame::EquipmentSlot renderEquipmentSlot(const TrainingEquipmentSlot slot) {
    switch (slot) {
        case TrainingEquipmentSlot::Primary:
            return renderer::RenderFrame::EquipmentSlot::Primary;
        case TrainingEquipmentSlot::Melee:
            return renderer::RenderFrame::EquipmentSlot::Melee;
        case TrainingEquipmentSlot::Throwable:
            return renderer::RenderFrame::EquipmentSlot::Throwable;
    }
    return renderer::RenderFrame::EquipmentSlot::Primary;
}

}  // namespace

Application::Application(const ApplicationLaunchMode launchMode)
    : launchMode_(launchMode) {}

Application::~Application() = default;

int Application::run() {
    if (!initialize()) {
        shutdown();
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto previous = clock::now();

    while (!window_->shouldClose()) {
        const auto now = clock::now();
        const std::chrono::duration<float> delta = now - previous;
        previous = now;

        window_->pollEvents();
        tick(delta.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    shutdown();
    return 0;
}

bool Application::initialize() {
    spdlog::info("[Init] 正在加载设置与资源目录...");
    assetRoot_ = assetRootPath();
    settingsPath_ = projectRoot_ / "settings.cfg";

    settings_ = loadSettings(settingsPath_);
    saveSettings(settings_, settingsPath_);

    spdlog::info("[Init] 正在引导项目资源...");
    bootstrapProjectFiles();
    spdlog::info("[Init] 正在创建默认场景...");
    activeMap_ = gameplay::makeDefaultBombDefusalMap(assetRoot_);
    contentDatabase_.resolveMapData(activeMap_);
    spdlog::info("[Init] 场景创建完毕: {} ({}x{})", activeMap_.name, activeMap_.width, activeMap_.depth);

    spdlog::info("[Init] 正在创建离线模拟世界...");
    simulation_ = gameplay::makeOfflinePracticeWorld(activeMap_);
    spdlog::info("[Init] 离线模拟世界创建完毕，当前角色数: {}", simulation_.players().size());
    networkSession_ = network::NetworkSession({
        .type = network::SessionType::Offline,
        .endpoint = {settings_.network.defaultServerHost, settings_.network.port},
        .maxPeers = static_cast<std::size_t>(std::max(2, settings_.network.maxPlayers)),
        .localPlayerId = makeSessionLocalPlayerId(network::SessionType::Offline, settings_.network.playerName),
        .localPlayerDisplayName = settings_.network.playerName,
    });

    spdlog::info("[Init] 正在创建窗口...");
    std::string initialWindowTitle = mainMenu_.title();
    if (launchMode_ == ApplicationLaunchMode::Editor) {
        initialWindowTitle += " | 地图编辑器";
    }
    window_ = platform::createWindow();
    if (!window_ || !window_->create({settings_.video.width, settings_.video.height, initialWindowTitle})) {
        spdlog::error("Failed to create game window.");
        return false;
    }
    syncInputMode();

    spdlog::info("[Init] 正在初始化渲染器...");
    renderer_ = renderer::createRenderer();
    if (!renderer_ || !renderer_->initialize(*window_)) {
        spdlog::error("Failed to initialize renderer.");
        return false;
    }

    spdlog::info("[Init] 正在初始化音频系统...");
    audioSystem_.initialize(settings_.audio.master, settings_.audio.effects);

    spdlog::info("[Init] 正在启动网络会话...");
    if (!networkSession_.start()) {
        spdlog::error("Failed to initialize networking session.");
        return false;
    }

    spdlog::info("Loaded {} weapons, {} material profiles, {} selectable models ({} archived), {} textures and {} selectable materials.",
        contentDatabase_.weapons().size(),
        contentDatabase_.materials().size(),
        contentDatabase_.assetManifest().editorModels.size(),
        contentDatabase_.assetManifest().models.size() - contentDatabase_.assetManifest().editorModels.size(),
        contentDatabase_.assetManifest().textures.size(),
        contentDatabase_.assetManifest().editorMaterials.size());
    initializeLaunchFlow();
    refreshWindowTitle();
    logCurrentFlow();
    return true;
}

void Application::initializeLaunchFlow() {
    if (launchMode_ == ApplicationLaunchMode::Editor) {
        mapBrowserTargetFlow_ = AppFlow::MapEditor;
        refreshMapCatalog();
        if (!mapCatalogPaths_.empty()) {
            loadEditorMapByIndex(activeMapCatalogIndex_);
        } else {
            createNewEditorMap();
        }
        currentFlow_ = AppFlow::MapEditor;
        mapEditorStatus_ = std::string("正在编辑地图: ") + activeMapPath_.stem().string();
        syncInputMode();
        return;
    }

    initializeSinglePlayerView();
}

void Application::tick(const float deltaSeconds) {
    handleInput();
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        updateSinglePlayerView(lastInput_, deltaSeconds);
        const bool localGameplayReady = !isRemoteClientSession() || multiplayerGameplayReady_;
        if (localGameplayReady && isAuthoritativeGameplaySession()) {
            simulation_.tick(deltaSeconds);
            syncLocalPlayerSimulationState();
        }
        if (localGameplayReady) {
            networkSession_.setLocalPlayerState(buildNetworkLocalPlayerState());
        }
        networkSession_.update(simulation_);
        if (isRemoteClientSession()) {
            if (networkSession_.remoteSessionEnded()) {
                returnToMainMenu(networkSession_.remoteSessionEndReason());
                return;
            }
            applyLatestNetworkMapState();
            if (multiplayerGameplayReady_) {
                applyLatestNetworkSnapshot();
            }
        }
        needsRedraw_ = true;
    } else if (currentFlow_ == AppFlow::MapEditor) {
        updateMapEditorView(lastInput_, deltaSeconds);
    }

    const bool continuousImGuiFrame =
        currentFlow_ == AppFlow::MultiPlayerLobby ||
        currentFlow_ == AppFlow::MapEditor;
    if (continuousImGuiFrame) {
        needsRedraw_ = true;
    }

    if (!needsRedraw_) {
        return;
    }

    const content::WeaponDefinition* previewWeapon = nullptr;
    std::string previewLabel = activeWeaponLabel_;
    switch (activeTrainingSlot_) {
        case TrainingEquipmentSlot::Primary:
            if (!trainingWeaponIds_.empty()) {
                previewWeapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
            }
            previewLabel = activeWeaponLabel_;
            break;
        case TrainingEquipmentSlot::Melee:
            previewWeapon = findWeaponDefinition("combat_knife");
            previewLabel = meleeWeaponLabel_;
            break;
        case TrainingEquipmentSlot::Throwable:
            if (selectedThrowableIndex_ % 3 == 0) {
                previewWeapon = findWeaponDefinition("frag");
            } else if (selectedThrowableIndex_ % 3 == 1) {
                previewWeapon = findWeaponDefinition("flashbang");
            } else {
                previewWeapon = findWeaponDefinition("smoke");
            }
            previewLabel = throwableLabel(selectedThrowableIndex_);
            break;
    }

    std::string editorCellFloorLabel = "无";
    std::string editorCellCoverLabel = "无";
    std::string editorCellPropLabel = "无";
    const util::Vec3 editorQueryPosition = mapEditorHasTarget_
        ? mapEditorTargetPosition_
        : centerOfCell(mapEditorCursorX_, mapEditorCursorZ_, 0.0f);
    bool hasAttackerSpawnAtCursor = false;
    bool hasDefenderSpawnAtCursor = false;
    if (currentFlow_ == AppFlow::MapEditor || currentFlow_ == AppFlow::MapBrowser) {
        for (const auto& prop : activeMap_.props) {
            if (!pointHitsPropFootprint(prop, editorQueryPosition.x, editorQueryPosition.z)) {
                continue;
            }
            const std::string label = propDisplayLabel(prop);
            if (label.find("地面") != std::string::npos) {
                editorCellFloorLabel = label;
            } else if (label.find("墙") != std::string::npos || label.find("掩体") != std::string::npos) {
                editorCellCoverLabel = label;
            } else if (editorCellPropLabel == "无") {
                editorCellPropLabel = label;
            }
        }
        for (const auto& spawn : activeMap_.spawns) {
            const float dx = spawn.position.x - editorQueryPosition.x;
            const float dz = spawn.position.z - editorQueryPosition.z;
            if (dx * dx + dz * dz > 0.80f * 0.80f) {
                continue;
            }
            if (spawn.team == gameplay::Team::Attackers) {
                hasAttackerSpawnAtCursor = true;
            } else if (spawn.team == gameplay::Team::Defenders) {
                hasDefenderSpawnAtCursor = true;
            }
        }
    }
    std::string editorCellSpawnLabel = "无";
    if (hasAttackerSpawnAtCursor && hasDefenderSpawnAtCursor) {
        editorCellSpawnLabel = "进攻/防守出生点";
    } else if (hasAttackerSpawnAtCursor) {
        editorCellSpawnLabel = "进攻出生点";
    } else if (hasDefenderSpawnAtCursor) {
        editorCellSpawnLabel = "防守出生点";
    }

    const gameplay::MapProp* selectedEditorProp =
        currentFlow_ == AppFlow::MapEditor ? selectedMapEditorProp() : nullptr;
    if (selectedEditorProp != nullptr) {
        editorCellPropLabel = propDisplayLabel(*selectedEditorProp);
    }
    const bool editorWantsPreviewProp =
        currentFlow_ == AppFlow::MapEditor &&
        mapEditorTool_ == MapEditorTool::Place &&
        mapEditorHasTarget_ &&
        (mapEditorPlacementKind_ == MapEditorPlacementKind::Wall ||
         mapEditorPlacementKind_ == MapEditorPlacementKind::Prop);
    const bool editorWantsPreviewSpawn =
        currentFlow_ == AppFlow::MapEditor &&
        mapEditorTool_ == MapEditorTool::Place &&
        mapEditorHasTarget_ &&
        (mapEditorPlacementKind_ == MapEditorPlacementKind::AttackerSpawn ||
         mapEditorPlacementKind_ == MapEditorPlacementKind::DefenderSpawn);
    const gameplay::MapProp previewEditorProp = editorWantsPreviewProp
        ? buildMapEditorPlacementPreviewProp()
        : gameplay::MapProp{};
    const gameplay::SpawnPoint previewEditorSpawn = editorWantsPreviewSpawn
        ? buildMapEditorPlacementPreviewSpawn()
        : gameplay::SpawnPoint{};
    const util::Vec3 renderCameraPosition =
        currentFlow_ == AppFlow::MapEditor ? mapEditorCameraPosition_ : singlePlayerCameraPosition_;
    const float renderCameraYawRadians =
        currentFlow_ == AppFlow::MapEditor ? mapEditorCameraYawRadians_ : (singlePlayerCameraYawRadians_ + aimYawOffsetRadians_);
    const float renderCameraPitchRadians =
        currentFlow_ == AppFlow::MapEditor ? mapEditorCameraPitchRadians_ : singlePlayerCameraPitchRadians_;

    const content::CharacterDefinition* playerCharacter = contentDatabase_.defaultCharacter();
    const content::ObjectAssetDefinition* selectedEditorObjectAsset = selectedMapEditorObjectAsset();
    const content::ObjectAssetDefinition* managedObjectAsset = selectedManagedObjectAsset();
    const std::vector<const content::ObjectAssetDefinition*> selectableEditorObjects = mapEditorSelectableObjects();
    std::set<std::string> selectableEditorCategories;
    for (const auto* object : selectableEditorObjects) {
        if (object != nullptr) {
            selectableEditorCategories.insert(object->category);
        }
    }
    const auto joinManagedTags = [](const std::vector<std::string>& tags) {
        std::ostringstream out;
        for (std::size_t index = 0; index < tags.size(); ++index) {
            if (index > 0) {
                out << ", ";
            }
            out << tags[index];
        }
        return out.str();
    };
    const int managedObjectActiveMapRefCount =
        managedObjectAsset != nullptr ? countObjectAssetReferencesInMap(activeMap_, managedObjectAsset->id) : 0;
    const int managedObjectStoredMapRefCount =
        managedObjectAsset != nullptr ? countObjectAssetReferencesInStoredMaps(managedObjectAsset->id) : 0;

    renderer::RenderFrame frame{
        .appFlow = currentFlow_,
        .mainMenu = currentFlow_ == AppFlow::MainMenu ? &mainMenu_ : nullptr,
        .world = &simulation_,
        .editingMap = currentFlow_ == AppFlow::MapEditor ? &activeMap_ : nullptr,
        .localPlayerId = networkSession_.localPlayerId(),
        .selectedMenuIndex = selectedMenuIndex_,
        .cameraPosition = renderCameraPosition,
        .cameraYawRadians = renderCameraYawRadians,
        .cameraPitchRadians = renderCameraPitchRadians,
        .activeWeaponLabel = activeWeaponLabel_,
        .ammoInMagazine = ammoInMagazine_,
        .reserveAmmo = reserveAmmo_,
        .eliminations = eliminations_,
        .lastShotHit = hitFlashSeconds_ > 0.0f,
        .crosshairSpread = crosshairSpreadDegrees_,
        .recoilKick = viewKickAmount_,
        .muzzleFlash = muzzleFlashSeconds_,
        .activeEquipmentSlot = renderEquipmentSlot(activeTrainingSlot_),
        .meleeWeaponLabel = meleeWeaponLabel_,
        .selectedThrowableLabel = throwableLabel(selectedThrowableIndex_),
        .activeOpticLabel = opticLabel(activeOptic_),
        .activeEquipmentModelPath = previewWeapon != nullptr ? previewWeapon->assets.modelPath : std::filesystem::path{},
        .activeEquipmentAlbedoPath = previewWeapon != nullptr ? previewWeapon->assets.albedoPath : std::filesystem::path{},
        .activeEquipmentMaterialPath = previewWeapon != nullptr ? previewWeapon->assets.materialPath : std::filesystem::path{},
        .activeEquipmentDisplayLabel = previewLabel,
        .playerCharacterModelPath = playerCharacter != nullptr ? playerCharacter->assets.modelPath : std::filesystem::path{},
        .playerCharacterAlbedoPath = playerCharacter != nullptr ? playerCharacter->assets.albedoPath : std::filesystem::path{},
        .playerCharacterMaterialPath = playerCharacter != nullptr ? playerCharacter->assets.materialPath : std::filesystem::path{},
        .playerCharacterScale = playerCharacter != nullptr ? playerCharacter->modelScale : 1.0f,
        .playerCharacterYawOffsetRadians = playerCharacter != nullptr ? playerCharacter->yawOffsetRadians : 0.0f,
        .fragCount = fragCount_,
        .flashCount = flashCount_,
        .smokeCount = smokeCount_,
        .flashOverlay = flashOverlaySeconds_,
        .smokeOverlay = smokeOverlaySeconds_,
        .opticMagnification = opticMagnification(activeOptic_),
        .selectedSettingsIndex = selectedSettingsIndex_,
        .settingsMouseSensitivity = settings_.gameplay.mouseSensitivity,
        .settingsMouseVerticalSensitivity = settings_.gameplay.mouseVerticalSensitivity,
        .settingsMaxLookPitchDegrees = settings_.gameplay.maxLookPitchDegrees,
        .settingsAutoReload = settings_.gameplay.autoReload,
        .editorHasTarget = mapEditorHasTarget_,
        .editorTargetOnSurface = mapEditorTargetOnSurface_,
        .editorTargetPosition = mapEditorTargetPosition_,
        .editorTargetNormal = mapEditorTargetNormal_,
        .editorCursorX = mapEditorCursorX_,
        .editorCursorZ = mapEditorCursorZ_,
        .editorToolLabel = mapEditorToolLabel(),
        .editorPlacementKindLabel = mapEditorPlacementKindLabel(),
        .editorViewModeLabel = mapEditorViewModeLabel(),
        .editorStatusLabel = mapEditorStatus_,
        .editorMapFileLabel = activeMapPath_.stem().string(),
        .editorSidebarWidth = settings_.video.editorSidebarWidth,
        .editorMouseLookActive = mapEditorMouseLookActive_,
        .editorIsOrthoView = mapEditorViewMode_ == MapEditorViewMode::Ortho25D,
        .editorOrthoSpan = mapEditorOrthoSpan_,
        .editorShowMeshOutline = mapEditorShowMeshOutline_,
        .editorShowCollisionOutline = mapEditorShowCollisionOutline_,
        .editorShowBoundingBox = mapEditorShowBoundingBox_,
        .editorUndoAvailable = !mapEditorUndoStack_.empty(),
        .editorMapIndex = activeMapCatalogIndex_,
        .editorMapCount = mapCatalogPaths_.size(),
        .editorAssetManifestLabel = contentDatabase_.objectCatalog().catalogPath.generic_string(),
        .editorObjectAssetCount = static_cast<int>(selectableEditorObjects.size()),
        .editorObjectCategoryCount = static_cast<int>(selectableEditorCategories.size()),
        .editorObjectAssets = &contentDatabase_.objectAssets(),
        .editorSelectedObjectAssetIndex = editorObjectAssetIndex_,
        .editorSelectedObjectAssetLabel = selectedEditorObjectAsset != nullptr ? selectedEditorObjectAsset->label : "无可用对象",
        .editorManagedObjectAssetIndex = managedObjectAssetIndex_,
        .editorHasManagedObjectAsset = managedObjectAsset != nullptr,
        .editorManagedObjectAssetId = managedObjectAsset != nullptr ? managedObjectAsset->id : std::string{},
        .editorManagedObjectAssetLabel = managedObjectAsset != nullptr ? managedObjectAsset->label : std::string{},
        .editorManagedObjectAssetCategory = managedObjectAsset != nullptr ? managedObjectAsset->category : std::string{},
        .editorManagedObjectModelPath = managedObjectAsset != nullptr ? managedObjectAsset->modelPath.generic_string() : std::string{},
        .editorManagedObjectMaterialPath = managedObjectAsset != nullptr ? managedObjectAsset->materialPath.generic_string() : std::string{},
        .editorManagedObjectTags = managedObjectAsset != nullptr ? joinManagedTags(managedObjectAsset->tags) : std::string{},
        .editorManagedObjectCollisionHalfExtents = managedObjectAsset != nullptr ? managedObjectAsset->collisionHalfExtents : util::Vec3{},
        .editorManagedObjectCollisionCenterOffset = managedObjectAsset != nullptr ? managedObjectAsset->collisionCenterOffset : util::Vec3{},
        .editorManagedObjectPreviewColor = managedObjectAsset != nullptr
            ? util::Vec3{
                static_cast<float>(managedObjectAsset->previewColor.r),
                static_cast<float>(managedObjectAsset->previewColor.g),
                static_cast<float>(managedObjectAsset->previewColor.b)}
            : util::Vec3{},
        .editorManagedObjectPlacementKind = managedObjectAsset != nullptr
            ? (managedObjectAsset->placementKind == content::ObjectPlacementKind::Wall ? 1 : 0)
            : 0,
        .editorManagedObjectActiveMapRefCount = managedObjectActiveMapRefCount,
        .editorManagedObjectStoredMapRefCount = managedObjectStoredMapRefCount,
        .editorManagedObjectCylindrical = managedObjectAsset != nullptr ? managedObjectAsset->cylindricalFootprint : false,
        .editorManagedObjectEditorVisible = managedObjectAsset != nullptr ? managedObjectAsset->editorVisible : true,
        .editorCellFloorLabel = editorCellFloorLabel,
        .editorCellCoverLabel = editorCellCoverLabel,
        .editorCellPropLabel = editorCellPropLabel,
        .editorCellSpawnLabel = editorCellSpawnLabel,
        .editorPropCount = static_cast<int>(activeMap_.props.size()),
        .editorSpawnCount = static_cast<int>(activeMap_.spawns.size()),
        .hoveredEditorPropIndex = hoveredEditorPropIndex_.has_value() ? static_cast<int>(*hoveredEditorPropIndex_) : -1,
        .hoveredEditorSpawnIndex = hoveredEditorSpawnIndex_.has_value() ? static_cast<int>(*hoveredEditorSpawnIndex_) : -1,
        .eraseEditorPropIndex = mapEditorTool_ == MapEditorTool::Erase && hoveredEditorPropIndex_.has_value()
            ? static_cast<int>(*hoveredEditorPropIndex_)
            : -1,
        .eraseEditorSpawnIndex = mapEditorTool_ == MapEditorTool::Erase && hoveredEditorSpawnIndex_.has_value()
            ? static_cast<int>(*hoveredEditorSpawnIndex_)
            : -1,
        .editorPlacementPreviewKind = editorWantsPreviewProp
            ? renderer::RenderFrame::EditorPlacementPreviewKind::Prop
            : (editorWantsPreviewSpawn
                ? renderer::RenderFrame::EditorPlacementPreviewKind::Spawn
                : renderer::RenderFrame::EditorPlacementPreviewKind::None),
        .editorPlacementPreviewProp = previewEditorProp,
        .editorPlacementPreviewSpawn = previewEditorSpawn,
        .selectedEditorPropIndex = selectedEditorPropIndex_.has_value() ? static_cast<int>(*selectedEditorPropIndex_) : -1,
        .hasSelectedEditorProp = selectedEditorProp != nullptr,
        .selectedEditorPropLabel = selectedEditorProp != nullptr ? propDisplayLabel(*selectedEditorProp) : std::string{},
        .selectedEditorPropAssetId = selectedEditorProp != nullptr ? selectedEditorProp->id : std::string{},
        .selectedEditorPropCategoryLabel = selectedEditorProp != nullptr ? selectedEditorProp->category : std::string{},
        .selectedEditorPropModelLabel = selectedEditorProp != nullptr ? pathDisplayLabel(selectedEditorProp->modelPath) : std::string{},
        .selectedEditorPropMaterialLabel = selectedEditorProp != nullptr ? pathDisplayLabel(selectedEditorProp->materialPath) : std::string{},
        .selectedEditorPropPosition = selectedEditorProp != nullptr ? selectedEditorProp->position : util::Vec3{},
        .selectedEditorPropRotationDegrees = selectedEditorProp != nullptr ? selectedEditorProp->rotationDegrees : util::Vec3{},
        .selectedEditorPropScale = selectedEditorProp != nullptr ? selectedEditorProp->scale : util::Vec3{1.0f, 1.0f, 1.0f},
        .mapBrowserTitle = mapBrowserTargetFlow_ == AppFlow::MapEditor ? "选择要编辑的地图" : "选择训练地图",
        .mapBrowserSubtitle = mapBrowserTargetFlow_ == AppFlow::MapEditor
            ? "同一套地图资源会被单机模式与编辑器共同使用。"
            : (mapBrowserTargetFlow_ == AppFlow::MultiPlayerLobby
                ? "先选地图，再进入联机房间参数页。"
                : "选择一张地图后进入本地训练场或机器人对战。"),
        .mapBrowserStatus = mapEditorStatus_,
        .mapBrowserItems = {},
        .mapBrowserSelectedIndex = activeMapCatalogIndex_,
        .multiplayerMapLabel = activeMap_.name,
        .multiplayerSessionTypeLabel = sessionTypeLabel(multiplayerSessionType_),
        .multiplayerSessionTypeIndex = multiplayerSessionType_ == network::SessionType::Client ? 1 : 0,
        .multiplayerHost = settings_.network.defaultServerHost,
        .multiplayerPort = static_cast<int>(settings_.network.port),
        .multiplayerEndpointLabel = settings_.network.defaultServerHost + ":" + std::to_string(settings_.network.port),
        .multiplayerMaxPlayers = settings_.network.maxPlayers,
        .multiplayerSelectedIndex = selectedMultiplayerIndex_,
        .multiplayerStatusLabel = multiplayerStatus_,
        .multiplayerSessionActive = multiplayerSessionActive_,
    };
    frame.mapBrowserItems.reserve(mapCatalogPaths_.size());
    for (const auto& path : mapCatalogPaths_) {
        frame.mapBrowserItems.push_back(path.stem().string());
    }
    renderer_->render(frame);
    needsRedraw_ = false;
    handleRendererUiActions();
}

void Application::shutdown() {
    networkSession_.stop();
    physicsWorld_.shutdown();
    audioSystem_.shutdown();
    if (renderer_) {
        renderer_->shutdown();
    }
}

void Application::bootstrapProjectFiles() {
    util::FileSystem::ensureDirectory(assetRoot_ / "generated");
    util::FileSystem::ensureDirectory(assetRoot_ / "maps");
    contentDatabase_.bootstrap(assetRoot_);
    clampMapEditorAssetSelection();
    clampManagedObjectAssetSelection();
    const auto& objectAssets = contentDatabase_.objectAssets();
    const auto crateObjectIt = std::find_if(objectAssets.begin(), objectAssets.end(), [](const content::ObjectAssetDefinition& object) {
        return object.id == "wooden_crate";
    });
    if (crateObjectIt != objectAssets.end()) {
        editorObjectAssetIndex_ = static_cast<std::size_t>(std::distance(objectAssets.begin(), crateObjectIt));
        managedObjectAssetIndex_ = editorObjectAssetIndex_;
    }
    clampMapEditorAssetSelection();

    spdlog::info("[Init] 正在导出场景文件与预览图...");
    activeMap_ = gameplay::makeDefaultBombDefusalMap(assetRoot_);
    contentDatabase_.resolveMapData(activeMap_);
    activeMapPath_ = assetRoot_ / "maps" / "depot_lab.arena";
    saveActiveMapArtifacts("初始化默认场景");
    {
        const auto previousMap = activeMap_;
        const auto previousPath = activeMapPath_;
        activeMap_ = gameplay::makeMetroStationShowcaseMap(assetRoot_);
        contentDatabase_.resolveMapData(activeMap_);
        activeMapPath_ = assetRoot_ / "maps" / "metro_platform.arena";
        saveActiveMapArtifacts("初始化 Metro 展示场景");
        activeMap_ = previousMap;
        activeMapPath_ = previousPath;
    }
    refreshMapCatalog();
    spdlog::info("[Init] 场景文件导出完毕: assets/maps/depot_lab.arena");
}

void Application::handleInput() {
    lastInput_ = window_->consumeInput();
    const bool inputActivity =
        lastInput_.upPressed || lastInput_.downPressed || lastInput_.leftPressed || lastInput_.rightPressed ||
        lastInput_.confirmPressed || lastInput_.backPressed || lastInput_.firePressed || lastInput_.jumpPressed ||
        lastInput_.reloadPressed || lastInput_.switchWeaponPressed || lastInput_.selectPrimaryPressed ||
        lastInput_.selectSecondaryPressed || lastInput_.selectMeleePressed || lastInput_.selectThrowablePressed ||
        lastInput_.selectToolFivePressed || lastInput_.cycleThrowablePressed || lastInput_.cycleOpticPressed ||
        lastInput_.editorSavePressed || lastInput_.editorDeletePressed || lastInput_.editorPreviousMapPressed ||
        lastInput_.editorNextMapPressed || lastInput_.editorNewMapPressed || lastInput_.editorUndoPressed ||
        lastInput_.editorToggleProjectionPressed || lastInput_.primaryClickPressed ||
        lastInput_.mouseDeltaX != 0 || lastInput_.mouseDeltaY != 0 || lastInput_.mouseWheelDelta != 0;
    if (renderer_ != nullptr) {
        if (renderer_->wantsKeyboardCapture()) {
            lastInput_.upPressed = false;
            lastInput_.downPressed = false;
            lastInput_.leftPressed = false;
            lastInput_.rightPressed = false;
            lastInput_.confirmPressed = false;
            lastInput_.backPressed = false;
            lastInput_.firePressed = false;
            lastInput_.jumpPressed = false;
            lastInput_.reloadPressed = false;
            lastInput_.switchWeaponPressed = false;
            lastInput_.selectPrimaryPressed = false;
            lastInput_.selectSecondaryPressed = false;
            lastInput_.selectMeleePressed = false;
            lastInput_.selectThrowablePressed = false;
            lastInput_.selectToolFivePressed = false;
            lastInput_.cycleThrowablePressed = false;
            lastInput_.cycleOpticPressed = false;
            lastInput_.editorSavePressed = false;
            lastInput_.editorDeletePressed = false;
            lastInput_.editorPreviousMapPressed = false;
            lastInput_.editorNextMapPressed = false;
            lastInput_.editorNewMapPressed = false;
            lastInput_.editorUndoPressed = false;
            lastInput_.editorToggleProjectionPressed = false;
            lastInput_.moveForwardHeld = false;
            lastInput_.moveBackwardHeld = false;
            lastInput_.moveUpHeld = false;
            lastInput_.moveDownHeld = false;
            lastInput_.strafeLeftHeld = false;
            lastInput_.strafeRightHeld = false;
            lastInput_.turnLeftHeld = false;
            lastInput_.turnRightHeld = false;
        }
        if (renderer_->wantsMouseCapture()) {
            lastInput_.primaryClickPressed = false;
            lastInput_.secondaryClickHeld = false;
            lastInput_.mouseDeltaX = 0;
            lastInput_.mouseDeltaY = 0;
            lastInput_.mouseWheelDelta = 0;
        }
    }
    const platform::InputSnapshot& input = lastInput_;
    if (inputActivity) {
        needsRedraw_ = true;
    }

    if (currentFlow_ == AppFlow::MainMenu) {
        if (input.primaryClickPressed) {
            const std::size_t hitItem = hitTestMainMenuItem(input.mouseX, input.mouseY);
            if (hitItem != std::numeric_limits<std::size_t>::max()) {
                activateMenuItem(hitItem);
            }
        }
        if (input.upPressed || input.leftPressed) {
            navigateMenu(-1);
        }
        if (input.downPressed || input.rightPressed) {
            navigateMenu(1);
        }
        if (input.confirmPressed) {
            activateSelectedMenuItem();
        }
        if (input.backPressed) {
            window_->requestClose();
        }
        return;
    }

    if (input.backPressed) {
        returnToMainMenu();
        return;
    }

    if (currentFlow_ == AppFlow::MapBrowser) {
        handleMapBrowserInput(input);
    } else if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        const bool allowAuthoritativeActions = isAuthoritativeGameplaySession();
        if (input.selectPrimaryPressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Primary);
        }
        if (input.selectMeleePressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Melee);
        }
        if (input.selectThrowablePressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Throwable);
        }
        if (input.cycleThrowablePressed) {
            cycleTrainingThrowable();
        }
        if (input.cycleOpticPressed) {
            cycleTrainingOptic();
        }
        if (allowAuthoritativeActions && (input.primaryClickPressed || input.firePressed)) {
            switch (activeTrainingSlot_) {
                case TrainingEquipmentSlot::Primary:
                    fireTrainingWeapon();
                    break;
                case TrainingEquipmentSlot::Melee:
                    useTrainingMelee();
                    break;
                case TrainingEquipmentSlot::Throwable:
                    useTrainingThrowable();
                    break;
            }
        }
        if (input.jumpPressed) {
            physicsWorld_.requestLocalPlayerJump();
        }
        if (allowAuthoritativeActions && input.reloadPressed && activeTrainingSlot_ == TrainingEquipmentSlot::Primary) {
            reloadTrainingWeapon();
        }
        if (allowAuthoritativeActions && input.switchWeaponPressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Primary);
            switchToNextTrainingWeapon();
        }
        if (allowAuthoritativeActions && input.firePressed) {
            spdlog::info("[SinglePlayer] local practice world ready, Esc 返回主菜单。");
        }
    } else if (currentFlow_ == AppFlow::MapEditor) {
        handleMapEditorInput(input);
    } else if (currentFlow_ == AppFlow::Settings) {
        if (input.upPressed) {
            navigateSettings(-1);
        }
        if (input.downPressed) {
            navigateSettings(1);
        }
        if (input.leftPressed) {
            adjustSelectedSetting(-1);
        }
        if (input.rightPressed) {
            adjustSelectedSetting(1);
        }
        if (input.confirmPressed) {
            activateSelectedSetting();
        }
    }
}

void Application::navigateMenu(const int delta) {
    const auto& items = mainMenu_.items();
    if (items.empty()) {
        return;
    }

    const int count = static_cast<int>(items.size());
    const int current = static_cast<int>(selectedMenuIndex_);
    selectedMenuIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    refreshWindowTitle();
    needsRedraw_ = true;
}

void Application::navigateSettings(const int delta) {
    const int count = static_cast<int>(kSettingsEntryCount);
    const int current = static_cast<int>(selectedSettingsIndex_);
    selectedSettingsIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    refreshWindowTitle();
    needsRedraw_ = true;
}

void Application::adjustSelectedSetting(const int delta) {
    bool changed = false;
    switch (selectedSettingsIndex_) {
        case 0: {
            const float next = std::clamp(
                std::round((settings_.gameplay.mouseSensitivity + static_cast<float>(delta) * 0.1f) * 10.0f) / 10.0f,
                0.1f, 3.0f);
            if (std::abs(next - settings_.gameplay.mouseSensitivity) > 0.001f) {
                settings_.gameplay.mouseSensitivity = next;
                changed = true;
            }
            break;
        }
        case 1: {
            const float next = std::clamp(
                std::round((settings_.gameplay.mouseVerticalSensitivity + static_cast<float>(delta) * 0.1f) * 10.0f) / 10.0f,
                0.5f, 3.0f);
            if (std::abs(next - settings_.gameplay.mouseVerticalSensitivity) > 0.001f) {
                settings_.gameplay.mouseVerticalSensitivity = next;
                changed = true;
            }
            break;
        }
        case 2: {
            const float next = std::clamp(
                std::round(settings_.gameplay.maxLookPitchDegrees + static_cast<float>(delta) * 2.0f),
                45.0f, 88.0f);
            if (std::abs(next - settings_.gameplay.maxLookPitchDegrees) > 0.001f) {
                settings_.gameplay.maxLookPitchDegrees = next;
                changed = true;
            }
            break;
        }
        case 3: {
            const bool next = delta > 0;
            if (next != settings_.gameplay.autoReload) {
                settings_.gameplay.autoReload = next;
                changed = true;
            }
            break;
        }
        default:
            break;
    }

    if (changed) {
        persistSettings("设置项已更新");
    }
}

void Application::activateSelectedSetting() {
    if (selectedSettingsIndex_ == 3) {
        settings_.gameplay.autoReload = !settings_.gameplay.autoReload;
        persistSettings("自动换弹已切换");
        return;
    }

    adjustSelectedSetting(1);
}

void Application::activateSelectedMenuItem() {
    activateMenuItem(selectedMenuIndex_);
}

void Application::activateMenuItem(const std::size_t index) {
    const MenuItem* item = mainMenu_.itemAt(index);
    if (item == nullptr) {
        return;
    }

    selectedMenuIndex_ = index;
    audioSystem_.play(audio::AudioCue::UiAccept);

    if (item->target == AppFlow::Exit) {
        window_->requestClose();
        return;
    }

    if (item->target == AppFlow::SinglePlayerLobby ||
        item->target == AppFlow::MapEditor) {
        openMapBrowser(item->target);
        return;
    }

    currentFlow_ = item->target;
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        initializeSinglePlayerView();
    } else if (currentFlow_ == AppFlow::MapEditor) {
        refreshMapCatalog();
        mapEditorCursorX_ = std::clamp(mapEditorCursorX_, 0, std::max(0, activeMap_.width - 1));
        mapEditorCursorZ_ = std::clamp(mapEditorCursorZ_, 0, std::max(0, activeMap_.depth - 1));
        initializeMapEditorView();
        mapEditorStatus_ = "地图编辑器已就绪";
    } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
        refreshMapCatalog();
        if (!mapCatalogPaths_.empty()) {
            loadEditorMapByIndex(activeMapCatalogIndex_);
            multiplayerStatus_ = std::string("已进入房间参数页，当前地图: ") + activeMapPath_.stem().string();
        } else {
            multiplayerStatus_ = "没有可用地图，请先创建或导入 .arena 地图";
        }
        selectedMultiplayerIndex_ = 0;
    } else if (currentFlow_ == AppFlow::Settings) {
        selectedSettingsIndex_ = 0;
    }
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::returnToMainMenu(const std::string_view statusOverride) {
    if (multiplayerSessionActive_) {
        networkSession_.stop(multiplayerSessionType_ == network::SessionType::Host
            ? "主机已返回主菜单"
            : std::string_view{});
        multiplayerSessionActive_ = false;
        multiplayerGameplayReady_ = true;
        receivedMultiplayerSnapshot_ = false;
        appliedNetworkMapRevision_ = 0;
        multiplayerStatus_ = statusOverride.empty()
            ? "尚未启动房间"
            : std::string(statusOverride);
    }

    if (launchMode_ == ApplicationLaunchMode::Editor) {
        if (currentFlow_ == AppFlow::MapBrowser) {
            if (window_ != nullptr) {
                window_->requestClose();
            }
            return;
        }

        openMapBrowser(AppFlow::MapEditor);
        mapEditorStatus_ = statusOverride.empty()
            ? "已返回地图列表"
            : std::string(statusOverride);
        needsRedraw_ = true;
        return;
    }

    currentFlow_ = AppFlow::MainMenu;
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::refreshWindowTitle() {
    const MenuItem* item = mainMenu_.itemAt(selectedMenuIndex_);
    std::string title = mainMenu_.title();
    title += " | ";

    if (currentFlow_ == AppFlow::MainMenu) {
        title += "菜单";
        if (item != nullptr) {
            title += " | 当前选中: ";
            title += item->label;
            title += " | 方向键/WASD 切换, Enter 确认, Esc 退出";
        }
    } else {
        switch (currentFlow_) {
            case AppFlow::MapBrowser: title += "地图选择"; break;
            case AppFlow::SinglePlayerLobby:
                title += multiplayerSessionActive_ ? "联机对局" : "单机模式";
                break;
            case AppFlow::MultiPlayerLobby: title += "联机模式"; break;
            case AppFlow::MapEditor: title += "地图编辑器"; break;
            case AppFlow::Settings: title += "设置"; break;
            case AppFlow::Exit: title += "退出"; break;
            case AppFlow::MainMenu: title += "菜单"; break;
        }
        if (currentFlow_ == AppFlow::Settings) {
            title += " | W/S 选择, A/D 调整, Enter 切换, Esc 返回";
        } else if (currentFlow_ == AppFlow::MapBrowser) {
            title += " | W/S 选择地图, Enter 确认, Q/E 切换, F6 新建, Esc 返回";
        } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
            title += " | 直接使用联机表单设置地图、地址和端口, Esc 返回";
        } else if (currentFlow_ == AppFlow::MapEditor) {
            title += " | 1选择 2放置 3擦除 G切换放置类型 O切换视图 Ctrl+Z撤销 F5保存 Esc返回";
        } else {
            title += " | Esc 返回主菜单";
        }
    }

    window_->setTitle(title);
}

void Application::syncInputMode() {
    if (window_ != nullptr) {
        window_->setRelativeMouseMode(
            currentFlow_ == AppFlow::SinglePlayerLobby ||
            (currentFlow_ == AppFlow::MapEditor && mapEditorMouseLookActive_));
    }
}

void Application::logCurrentFlow() const {
    switch (currentFlow_) {
        case AppFlow::MainMenu:
            spdlog::info("[UI] Main menu ready. Use Arrow Keys/WASD + Enter.");
            break;
        case AppFlow::MapBrowser:
            spdlog::info("[UI] Entered MapBrowser. Use W/S, Q/E, Enter, F6.");
            break;
        case AppFlow::SinglePlayerLobby:
            spdlog::info("[UI] Entered {}.",
                multiplayerSessionActive_
                    ? (multiplayerSessionType_ == network::SessionType::Host ? "MultiPlayerMatchHost" : "MultiPlayerMatchClient")
                    : "SinglePlayerLobby");
            break;
        case AppFlow::MultiPlayerLobby:
            spdlog::info("[UI] Entered MultiPlayerLobby. Use ImGui form to choose map, host, port and room settings.");
            break;
        case AppFlow::MapEditor:
            spdlog::info("[UI] Entered MapEditor. 1选择 2放置 3擦除, G切换放置类型, O切换正交视图, Ctrl+Z撤销, F5保存.");
            break;
        case AppFlow::Settings:
            spdlog::info("[UI] Entered Settings. Use W/S to select, A/D to adjust, Enter to toggle.");
            break;
        case AppFlow::Exit:
            spdlog::info("[UI] Exit requested.");
            break;
    }
}

void Application::handleRendererUiActions() {
    if (renderer_ == nullptr) {
        return;
    }

    const auto trimText = [](std::string value) {
        auto isSpace = [](unsigned char character) {
            return std::isspace(character) != 0;
        };
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
        return value;
    };
    const auto splitTags = [&](std::string value) {
        std::vector<std::string> tags;
        std::string currentTag;
        auto flush = [&]() {
            const std::string trimmed = trimText(currentTag);
            if (!trimmed.empty()) {
                tags.push_back(trimmed);
            }
            currentTag.clear();
        };
        for (char character : value) {
            if (character == ',' || character == ';' || character == '\n') {
                flush();
            } else {
                currentTag.push_back(character);
            }
        }
        flush();
        return tags;
    };
    const auto updateManagedObject = [&](const auto& mutator) {
        if (currentFlow_ != AppFlow::MapEditor) {
            return;
        }
        const content::ObjectAssetDefinition* current = selectedManagedObjectAsset();
        if (current == nullptr) {
            mapEditorStatus_ = "当前没有可编辑的对象资产";
            needsRedraw_ = true;
            return;
        }
        content::ObjectAssetDefinition definition = *current;
        mutator(definition);
        saveManagedObjectAsset(definition, current->id);
    };

    for (const renderer::UiAction& action : renderer_->consumeUiActions()) {
        switch (action.type) {
            case renderer::UiActionType::ActivateMainMenuItem:
                if (currentFlow_ == AppFlow::MainMenu && action.value0 >= 0) {
                    activateMenuItem(static_cast<std::size_t>(action.value0));
                }
                break;
            case renderer::UiActionType::SelectMapBrowserItem:
                if ((currentFlow_ == AppFlow::MapBrowser || currentFlow_ == AppFlow::MultiPlayerLobby || currentFlow_ == AppFlow::MapEditor) &&
                    action.value0 >= 0) {
                    loadEditorMapByIndex(static_cast<std::size_t>(action.value0));
                    if (currentFlow_ == AppFlow::MultiPlayerLobby) {
                        multiplayerStatus_ = std::string("已选择地图: ") + activeMapPath_.stem().string();
                        needsRedraw_ = true;
                    } else if (currentFlow_ == AppFlow::MapEditor) {
                        mapEditorStatus_ = std::string("已打开地图: ") + activeMapPath_.stem().string();
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::ActivateCurrentMapBrowserItem:
                if (currentFlow_ == AppFlow::MapBrowser) {
                    activateSelectedMapBrowserItem();
                }
                break;
            case renderer::UiActionType::CycleMapBrowser:
                if (currentFlow_ == AppFlow::MapBrowser && action.value0 != 0) {
                    cycleEditorMap(action.value0);
                }
                break;
            case renderer::UiActionType::CreateMapBrowserMap:
                if (currentFlow_ == AppFlow::MapBrowser) {
                    createNewEditorMap();
                    if (mapBrowserTargetFlow_ == AppFlow::MapEditor) {
                        currentFlow_ = AppFlow::MapEditor;
                        syncInputMode();
                        refreshWindowTitle();
                        logCurrentFlow();
                    }
                }
                break;
            case renderer::UiActionType::SelectMapEditorTool:
                if (currentFlow_ == AppFlow::MapEditor &&
                    action.value0 >= 0 &&
                    action.value0 <= static_cast<int>(MapEditorTool::Erase)) {
                    mapEditorTool_ = static_cast<MapEditorTool>(action.value0);
                    mapEditorStatus_ = std::string("当前工具: ") + mapEditorToolLabel();
                    if (mapEditorTool_ == MapEditorTool::Select) {
                        syncMapEditorTargetFromView();
                    }
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SelectSceneEditorProp:
                if (currentFlow_ == AppFlow::MapEditor && action.value0 >= 0) {
                    const std::size_t index = static_cast<std::size_t>(action.value0);
                    if (index < activeMap_.props.size()) {
                        focusMapEditorOnProp(index);
                        mapEditorStatus_ = std::string("已选中对象: ") + propDisplayLabel(activeMap_.props[index]);
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::SelectMapEditorPlacementKind:
                if (currentFlow_ == AppFlow::MapEditor &&
                    action.value0 >= 0 &&
                    action.value0 <= static_cast<int>(MapEditorPlacementKind::DefenderSpawn)) {
                    mapEditorPlacementKind_ = static_cast<MapEditorPlacementKind>(action.value0);
                    clampMapEditorAssetSelection();
                    mapEditorTool_ = MapEditorTool::Place;
                    mapEditorStatus_ = std::string("放置类型: ") + mapEditorPlacementKindLabel();
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SelectEditorObjectAsset:
                if (currentFlow_ == AppFlow::MapEditor && action.value0 >= 0) {
                    editorObjectAssetIndex_ = static_cast<std::size_t>(action.value0);
                    clampMapEditorAssetSelection();
                    if (const auto* object = selectedMapEditorObjectAsset(); object != nullptr) {
                        mapEditorStatus_ = std::string("当前对象: ") + object->label;
                    }
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SelectManagedObjectAsset:
                if (currentFlow_ == AppFlow::MapEditor && action.value0 >= 0) {
                    managedObjectAssetIndex_ = static_cast<std::size_t>(action.value0);
                    clampManagedObjectAssetSelection();
                    if (const auto* object = selectedManagedObjectAsset(); object != nullptr) {
                        mapEditorStatus_ = std::string("资产管理器当前对象: ") + object->label;
                    }
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::CreateManagedObjectAsset:
                if (currentFlow_ == AppFlow::MapEditor) {
                    createManagedObjectAsset();
                }
                break;
            case renderer::UiActionType::DeleteManagedObjectAsset:
                if (currentFlow_ == AppFlow::MapEditor) {
                    deleteManagedObjectAsset();
                }
                break;
            case renderer::UiActionType::SaveManagedObjectAsset:
                if (currentFlow_ == AppFlow::MapEditor) {
                    mapEditorStatus_ = "对象资产已同步保存";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SetManagedObjectAssetId:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.id = trimText(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetLabel:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.label = trimText(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetCategory:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.category = trimText(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetModelPath:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.modelPath = trimText(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetMaterialPath:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.materialPath = trimText(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetTags:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.tags = splitTags(action.text);
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetPlacementKind:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.placementKind = action.value0 == 1
                        ? content::ObjectPlacementKind::Wall
                        : content::ObjectPlacementKind::Prop;
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetCollisionHalfExtents:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.collisionHalfExtents = {
                        std::max(0.01f, std::abs(action.vectorValue.x)),
                        std::max(0.01f, std::abs(action.vectorValue.y)),
                        std::max(0.01f, std::abs(action.vectorValue.z)),
                    };
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetCollisionCenterOffset:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.collisionCenterOffset = action.vectorValue;
                });
                break;
            case renderer::UiActionType::SetManagedObjectAssetPreviewColor:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.previewColor = {
                        static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(std::lround(action.vectorValue.x)), 0, 255)),
                        static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(std::lround(action.vectorValue.y)), 0, 255)),
                        static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(std::lround(action.vectorValue.z)), 0, 255)),
                    };
                });
                break;
            case renderer::UiActionType::ToggleManagedObjectAssetCylindrical:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.cylindricalFootprint = action.value0 != 0;
                });
                break;
            case renderer::UiActionType::ToggleManagedObjectAssetEditorVisible:
                updateManagedObject([&](content::ObjectAssetDefinition& definition) {
                    definition.editorVisible = action.value0 != 0;
                });
                break;
            case renderer::UiActionType::SetSelectedEditorPropPosition:
                if (currentFlow_ == AppFlow::MapEditor) {
                    setSelectedMapEditorPropPosition(action.vectorValue);
                }
                break;
            case renderer::UiActionType::SetSelectedEditorPropRotation:
                if (currentFlow_ == AppFlow::MapEditor) {
                    setSelectedMapEditorPropRotation(action.vectorValue);
                }
                break;
            case renderer::UiActionType::SetSelectedEditorPropScale:
                if (currentFlow_ == AppFlow::MapEditor) {
                    setSelectedMapEditorPropScale(action.vectorValue);
                }
                break;
            case renderer::UiActionType::ToggleMapEditorProjection:
                if (currentFlow_ == AppFlow::MapEditor) {
                    switchMapEditorViewMode(
                        mapEditorViewMode_ == MapEditorViewMode::Perspective
                            ? MapEditorViewMode::Ortho25D
                            : MapEditorViewMode::Perspective);
                }
                break;
            case renderer::UiActionType::ToggleMapEditorMeshOutline:
                if (currentFlow_ == AppFlow::MapEditor) {
                    mapEditorShowMeshOutline_ = action.value0 != 0;
                    mapEditorStatus_ = mapEditorShowMeshOutline_ ? "已显示真实轮廓" : "已隐藏真实轮廓";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::ToggleMapEditorCollisionOutline:
                if (currentFlow_ == AppFlow::MapEditor) {
                    mapEditorShowCollisionOutline_ = action.value0 != 0;
                    mapEditorStatus_ = mapEditorShowCollisionOutline_ ? "已显示碰撞箱轮廓" : "已隐藏碰撞箱轮廓";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::ToggleMapEditorBoundingBox:
                if (currentFlow_ == AppFlow::MapEditor) {
                    mapEditorShowBoundingBox_ = action.value0 != 0;
                    mapEditorStatus_ = mapEditorShowBoundingBox_ ? "已显示最小包裹箱" : "已隐藏最小包裹箱";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::UndoMapEditorChange:
                if (currentFlow_ == AppFlow::MapEditor && !restoreMapEditorUndoSnapshot()) {
                    mapEditorStatus_ = "当前没有可撤销的操作";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::MoveMapEditorCursor:
                if (currentFlow_ == AppFlow::MapEditor) {
                    moveMapEditorCursor(action.value0, action.value1);
                }
                break;
            case renderer::UiActionType::ApplyMapEditorTool:
                if (currentFlow_ == AppFlow::MapEditor) {
                    applyMapEditorTool();
                }
                break;
            case renderer::UiActionType::EraseMapEditorCell:
                if (currentFlow_ == AppFlow::MapEditor) {
                    eraseMapEditorCell();
                }
                break;
            case renderer::UiActionType::CycleEditorMap:
                if (currentFlow_ == AppFlow::MapEditor && action.value0 != 0) {
                    cycleEditorMap(action.value0);
                }
                break;
            case renderer::UiActionType::CreateEditorMap:
                if (currentFlow_ == AppFlow::MapEditor) {
                    createNewEditorMap();
                }
                break;
            case renderer::UiActionType::SaveEditorMap:
                if (currentFlow_ == AppFlow::MapEditor) {
                    saveActiveMapArtifacts("ImGui 编辑器保存");
                    mapEditorStatus_ = "地图与预览图已保存";
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SetEditorSidebarWidth:
                if (currentFlow_ == AppFlow::MapEditor) {
                    const float nextWidth = std::clamp(static_cast<float>(action.value0), 320.0f, 860.0f);
                    if (std::abs(settings_.video.editorSidebarWidth - nextWidth) > 0.5f) {
                        settings_.video.editorSidebarWidth = nextWidth;
                        saveSettings(settings_, settingsPath_);
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::AdjustMultiplayerSetting:
                if (currentFlow_ == AppFlow::MultiPlayerLobby && action.value0 >= 0) {
                    selectedMultiplayerIndex_ = static_cast<std::size_t>(action.value0);
                    adjustSelectedMultiplayerSetting(action.value1);
                }
                break;
            case renderer::UiActionType::SetMultiplayerSessionType:
                if (currentFlow_ == AppFlow::MultiPlayerLobby) {
                    multiplayerSessionType_ = action.value0 == 1 ? network::SessionType::Client : network::SessionType::Host;
                    multiplayerStatus_ = std::string("联机模式切换为") + sessionTypeLabel(multiplayerSessionType_);
                    needsRedraw_ = true;
                }
                break;
            case renderer::UiActionType::SetMultiplayerHost:
                if (currentFlow_ == AppFlow::MultiPlayerLobby) {
                    const std::string nextHost = trimAscii(action.text);
                    if (nextHost.empty()) {
                        multiplayerStatus_ = "服务器地址不能为空";
                        needsRedraw_ = true;
                    } else if (nextHost != settings_.network.defaultServerHost) {
                        settings_.network.defaultServerHost = nextHost;
                        saveSettings(settings_, settingsPath_);
                        multiplayerStatus_ = std::string("服务器地址已更新: ") + nextHost;
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::SetMultiplayerPort:
                if (currentFlow_ == AppFlow::MultiPlayerLobby) {
                    const int nextPort = std::clamp(action.value0, 1, 65535);
                    if (nextPort != static_cast<int>(settings_.network.port)) {
                        settings_.network.port = static_cast<std::uint16_t>(nextPort);
                        saveSettings(settings_, settingsPath_);
                        multiplayerStatus_ = std::string("端口已更新: ") + std::to_string(nextPort);
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::SetMultiplayerMaxPlayers:
                if (currentFlow_ == AppFlow::MultiPlayerLobby) {
                    const int nextMaxPlayers = std::clamp(action.value0, 2, 32);
                    if (nextMaxPlayers != settings_.network.maxPlayers) {
                        settings_.network.maxPlayers = nextMaxPlayers;
                        saveSettings(settings_, settingsPath_);
                        multiplayerStatus_ = std::string("房间人数已更新: ") + std::to_string(nextMaxPlayers);
                        needsRedraw_ = true;
                    }
                }
                break;
            case renderer::UiActionType::ActivateMultiplayerSetting:
                if (currentFlow_ == AppFlow::MultiPlayerLobby && action.value0 >= 0) {
                    selectedMultiplayerIndex_ = static_cast<std::size_t>(action.value0);
                    activateSelectedMultiplayerSetting();
                }
                break;
            case renderer::UiActionType::AdjustSetting:
                if (currentFlow_ == AppFlow::Settings && action.value0 >= 0) {
                    selectedSettingsIndex_ = static_cast<std::size_t>(action.value0);
                    adjustSelectedSetting(action.value1);
                }
                break;
            case renderer::UiActionType::ActivateSetting:
                if (currentFlow_ == AppFlow::Settings && action.value0 >= 0) {
                    selectedSettingsIndex_ = static_cast<std::size_t>(action.value0);
                    activateSelectedSetting();
                }
                break;
            case renderer::UiActionType::ReturnToMainMenu:
                if (currentFlow_ != AppFlow::MainMenu) {
                    returnToMainMenu();
                }
                break;
        }
    }
}

void Application::initializeSinglePlayerView() {
    spdlog::info("[SinglePlayer] 正在创建训练场相机与出生点...");
    simulation_ = gameplay::makeOfflinePracticeWorld(activeMap_);
    spdlog::info("[SinglePlayer] 训练场世界已重新创建。");

    const gameplay::Team preferredTeam =
        multiplayerSessionActive_ && multiplayerSessionType_ == network::SessionType::Client
            ? gameplay::Team::Defenders
            : gameplay::Team::Attackers;
    singlePlayerCameraPosition_ = {3.0f, 1.0f, 3.0f};
    for (const auto& spawn : activeMap_.spawns) {
        if (spawn.team == preferredTeam) {
            singlePlayerCameraPosition_ = spawn.position;
            break;
        }
    }
    spdlog::info("[SinglePlayer] 已确定攻击方出生点: ({}, {}, {})",
        singlePlayerCameraPosition_.x,
        singlePlayerCameraPosition_.y,
        singlePlayerCameraPosition_.z);

    singlePlayerCameraPosition_.x += 0.2f;
    singlePlayerCameraPosition_.z += 0.2f;
    singlePlayerCameraYawRadians_ = 0.15f;
    singlePlayerCameraPitchRadians_ = 0.0f;
    spdlog::info("[SinglePlayer] 正在采样出生点地面高度...");
    const float sampledEyeHeight =
        gameplay::sampleFloorHeight(activeMap_, singlePlayerCameraPosition_.x, singlePlayerCameraPosition_.z) +
        kSinglePlayerEyeHeight;
    singlePlayerCameraPosition_.y = std::max(singlePlayerCameraPosition_.y, sampledEyeHeight);
    spdlog::info("[SinglePlayer] 出生点相机高度已确定: {}", singlePlayerCameraPosition_.y);
    const util::Vec3 localPlayerFeetPosition{
        singlePlayerCameraPosition_.x,
        singlePlayerCameraPosition_.y - kSinglePlayerEyeHeight,
        singlePlayerCameraPosition_.z,
    };
    spdlog::info("[SinglePlayer] 正在初始化 Jolt 物理世界...");
    physicsWorld_.shutdown();
    if (!physicsWorld_.initialize(assetRoot_, activeMap_, localPlayerFeetPosition)) {
        spdlog::warn("[Physics] Failed to initialize Jolt world for current singleplayer map.");
    } else {
        spdlog::info("[SinglePlayer] Jolt 物理世界初始化完成。");
    }

    trainingWeaponIds_.clear();
    spdlog::info("[SinglePlayer] 正在装配可用武器列表...");
    for (const auto& weapon : contentDatabase_.weapons()) {
        using content::WeaponCategory;
        if (weapon.category == WeaponCategory::Rifle ||
            weapon.category == WeaponCategory::SniperRifle ||
            weapon.category == WeaponCategory::SubmachineGun ||
            weapon.category == WeaponCategory::Shotgun) {
            trainingWeaponIds_.push_back(weapon.id);
        }
    }
    if (trainingWeaponIds_.empty()) {
        trainingWeaponIds_.push_back("ak12");
    }

    activeTrainingWeaponIndex_ = 0;
    fireCooldownSeconds_ = 0.0f;
    hitFlashSeconds_ = 0.0f;
    muzzleFlashSeconds_ = 0.0f;
    flashOverlaySeconds_ = 0.0f;
    smokeOverlaySeconds_ = 0.0f;
    aimYawOffsetRadians_ = 0.0f;
    viewKickAmount_ = 0.0f;
    eliminations_ = 0;
    activeTrainingSlot_ = TrainingEquipmentSlot::Primary;
    activeOptic_ = content::OpticType::RedDot;
    selectedThrowableIndex_ = 0;
    fragCount_ = 2;
    flashCount_ = 2;
    smokeCount_ = 2;
    if (const auto* weapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_])) {
        activeWeaponLabel_ = weapon->displayName;
        ammoInMagazine_ = weapon->magazineSize;
        reserveAmmo_ = weapon->reserveAmmo;
        crosshairSpreadDegrees_ = trainingBaseSpread(*weapon);
    }
    if (const gameplay::Entity localPlayer = simulation_.firstPlayerEntity(); localPlayer != gameplay::kNullEntity) {
        simulation_.withPlayer(localPlayer, [&](gameplay::PlayerComponents& player) {
            player.identity.id = networkSession_.localPlayerId();
            player.identity.displayName = settings_.network.playerName;
            player.team.value = preferredTeam;
        });
    }
    syncLocalPlayerSimulationState();
    multiplayerGameplayReady_ = true;
    if (!isRemoteClientSession()) {
        networkSession_.setHostMapState(activeMapPath_.stem().string(), activeMap_);
    }
    spdlog::info("[SinglePlayer] 训练场准备完毕，初始武器: {}，出生点: ({}, {}, {})",
        activeWeaponLabel_,
        singlePlayerCameraPosition_.x,
        singlePlayerCameraPosition_.y,
        singlePlayerCameraPosition_.z);
}

util::Vec3 Application::clampMapEditorPerspectiveCameraPosition(const util::Vec3& position) const {
    return {
        std::clamp(position.x, -8.0f, static_cast<float>(activeMap_.width) + 8.0f),
        std::clamp(position.y, 0.8f, static_cast<float>(std::max(activeMap_.height, 8)) + 20.0f),
        std::clamp(position.z, -8.0f, static_cast<float>(activeMap_.depth) + 8.0f),
    };
}

util::Vec3 Application::clampMapEditorOrthoFocusPosition(const util::Vec3& position) const {
    return clampEditorTargetPosition(activeMap_, position);
}

util::Vec3 Application::deriveMapEditorFocusPointFromPerspective() const {
    if (mapEditorHasTarget_) {
        return clampMapEditorOrthoFocusPosition(mapEditorTargetPosition_);
    }

    const util::Vec3 forward = util::cameraForwardVector(
        mapEditorPerspectiveCameraYawRadians_,
        mapEditorPerspectiveCameraPitchRadians_);
    RaySurfaceHit groundHit{};
    if (rayIntersectGroundPlane(
            activeMap_,
            mapEditorPerspectiveCameraPosition_,
            forward,
            kMapEditorMaxPlacementDistance,
            groundHit)) {
        return clampMapEditorOrthoFocusPosition(groundHit.point);
    }

    return clampMapEditorOrthoFocusPosition(
        defaultFloatingEditorTargetPosition(activeMap_, mapEditorPerspectiveCameraPosition_, forward));
}

void Application::syncMapEditorCameraState() {
    if (mapEditorViewMode_ == MapEditorViewMode::Perspective) {
        mapEditorPerspectiveCameraPosition_ = clampMapEditorPerspectiveCameraPosition(mapEditorPerspectiveCameraPosition_);
        mapEditorCameraPosition_ = mapEditorPerspectiveCameraPosition_;
        mapEditorCameraYawRadians_ = mapEditorPerspectiveCameraYawRadians_;
        mapEditorCameraPitchRadians_ = mapEditorPerspectiveCameraPitchRadians_;
        return;
    }

    mapEditorOrthoFocusPosition_ = clampMapEditorOrthoFocusPosition(mapEditorOrthoFocusPosition_);
    mapEditorCameraPosition_ = mapEditorOrthoFocusPosition_;
    mapEditorCameraYawRadians_ = util::mapEditorOrthoYawRadians();
    mapEditorCameraPitchRadians_ = util::mapEditorOrthoPitchRadians();
}

void Application::focusMapEditorOnProp(const std::size_t index) {
    if (index >= activeMap_.props.size()) {
        return;
    }

    const util::Vec3 focus = clampMapEditorOrthoFocusPosition(activeMap_.props[index].position);
    if (mapEditorViewMode_ == MapEditorViewMode::Ortho25D) {
        mapEditorOrthoFocusPosition_ = focus;
    } else {
        util::Vec3 nextPosition = addVec3(focus, mapEditorPerspectiveFocusOffset_);
        const float floorHeight = gameplay::sampleFloorHeight(activeMap_, nextPosition.x, nextPosition.z);
        nextPosition.y = std::max(nextPosition.y, floorHeight + 1.6f);
        mapEditorPerspectiveCameraPosition_ = clampMapEditorPerspectiveCameraPosition(nextPosition);
    }

    syncMapEditorCameraState();
    syncMapEditorTargetFromView();
    selectedEditorPropIndex_ = index;
}

void Application::switchMapEditorViewMode(const MapEditorViewMode nextMode) {
    if (mapEditorViewMode_ == nextMode) {
        syncMapEditorCameraState();
        syncMapEditorTargetFromView();
        return;
    }

    if (nextMode == MapEditorViewMode::Ortho25D) {
        const util::Vec3 focus = deriveMapEditorFocusPointFromPerspective();
        mapEditorOrthoFocusPosition_ = focus;
        mapEditorPerspectiveFocusOffset_ = subtractVec3(mapEditorPerspectiveCameraPosition_, focus);
    } else {
        const util::Vec3 focus = clampMapEditorOrthoFocusPosition(mapEditorOrthoFocusPosition_);
        util::Vec3 nextPosition = addVec3(focus, mapEditorPerspectiveFocusOffset_);
        const float floorHeight = gameplay::sampleFloorHeight(activeMap_, nextPosition.x, nextPosition.z);
        nextPosition.y = std::max(nextPosition.y, floorHeight + 1.6f);
        mapEditorPerspectiveCameraPosition_ = clampMapEditorPerspectiveCameraPosition(nextPosition);
    }

    mapEditorViewMode_ = nextMode;
    mapEditorMouseLookActive_ = false;
    syncInputMode();
    syncMapEditorCameraState();
    syncMapEditorTargetFromView();
    mapEditorStatus_ = std::string("视图已切换为: ") + mapEditorViewModeLabel();
    needsRedraw_ = true;
}

void Application::initializeMapEditorView() {
    util::Vec3 focus = centerOfCell(std::max(0, activeMap_.width / 2), std::max(0, activeMap_.depth / 2), 0.0f);
    for (const auto& spawn : activeMap_.spawns) {
        if (spawn.team == gameplay::Team::Attackers) {
            focus = spawn.position;
            break;
        }
    }

    focus = clampMapEditorOrthoFocusPosition(focus);
    const float focusFloor = gameplay::sampleFloorHeight(activeMap_, focus.x, focus.z);
    mapEditorPerspectiveCameraPosition_ = clampMapEditorPerspectiveCameraPosition({
        focus.x - 5.0f,
        std::max(3.0f, focusFloor + 3.4f),
        focus.z - 5.0f,
    });
    mapEditorPerspectiveCameraYawRadians_ = degreesToRadians(38.0f);
    mapEditorPerspectiveCameraPitchRadians_ = degreesToRadians(-20.0f);
    mapEditorPerspectiveFocusOffset_ = subtractVec3(mapEditorPerspectiveCameraPosition_, focus);
    mapEditorOrthoFocusPosition_ = focus;
    mapEditorOrthoSpan_ = std::max(
        8.0f,
        static_cast<float>(std::max(activeMap_.width, activeMap_.depth)) * 0.55f);
    mapEditorTargetPosition_ = focus;
    mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
    mapEditorHasTarget_ = true;
    mapEditorTargetOnSurface_ = true;
    mapEditorMouseLookActive_ = false;
    hoveredEditorPropIndex_.reset();
    hoveredEditorSpawnIndex_.reset();
    selectedEditorPropIndex_.reset();
    syncMapEditorCameraState();
    syncInputMode();
    syncMapEditorTargetFromView();
    spdlog::info("[MapEditor] 3D editor camera ready at ({}, {}, {}).",
        mapEditorCameraPosition_.x,
        mapEditorCameraPosition_.y,
        mapEditorCameraPosition_.z);
}

void Application::updateMapEditorView(const platform::InputSnapshot& input, const float deltaSeconds) {
    if (mapEditorViewMode_ == MapEditorViewMode::Perspective) {
        const bool wantsMouseLook =
            input.secondaryClickHeld &&
            (renderer_ == nullptr || !renderer_->wantsMouseCapture());
        if (wantsMouseLook != mapEditorMouseLookActive_) {
            mapEditorMouseLookActive_ = wantsMouseLook;
            syncInputMode();
            needsRedraw_ = true;
        }

        const float mouseSensitivity = std::clamp(settings_.gameplay.mouseSensitivity, 0.05f, 3.0f);
        const float mouseVerticalSensitivity = std::clamp(settings_.gameplay.mouseVerticalSensitivity, 0.5f, 3.0f);
        const float maxPitchRadians = degreesToRadians(88.0f);
        const float mouseYawScale = 0.0024f * mouseSensitivity;
        const float mousePitchScale = 0.0024f * mouseSensitivity * mouseVerticalSensitivity;
        if (mapEditorMouseLookActive_) {
            mapEditorPerspectiveCameraYawRadians_ += static_cast<float>(input.mouseDeltaX) * mouseYawScale;
            mapEditorPerspectiveCameraPitchRadians_ = std::clamp(
                mapEditorPerspectiveCameraPitchRadians_ + static_cast<float>(input.mouseDeltaY) * mousePitchScale,
                -maxPitchRadians,
                maxPitchRadians);
        }

        const util::Vec3 forward = util::cameraForwardVector(
            mapEditorPerspectiveCameraYawRadians_,
            mapEditorPerspectiveCameraPitchRadians_);
        const util::Vec3 forwardFlat = normalizeVec3({forward.x, 0.0f, forward.z});
        const util::Vec3 right = normalizeVec3({-forwardFlat.z, 0.0f, forwardFlat.x});

        util::Vec3 velocity{};
        if (input.moveForwardHeld) {
            velocity = addVec3(velocity, forwardFlat);
        }
        if (input.moveBackwardHeld) {
            velocity = subtractVec3(velocity, forwardFlat);
        }
        if (input.strafeRightHeld) {
            velocity = addVec3(velocity, right);
        }
        if (input.strafeLeftHeld) {
            velocity = subtractVec3(velocity, right);
        }
        if (input.moveUpHeld) {
            velocity.y += 1.0f;
        }
        if (input.moveDownHeld) {
            velocity.y -= 1.0f;
        }

        const float speed = 8.5f;
        if (lengthSquaredVec3(velocity) > 0.0001f) {
            mapEditorPerspectiveCameraPosition_ = clampMapEditorPerspectiveCameraPosition(
                addVec3(
                    mapEditorPerspectiveCameraPosition_,
                    multiplyVec3(normalizeVec3(velocity), speed * deltaSeconds)));
            needsRedraw_ = true;
        }
    } else {
        if (mapEditorMouseLookActive_) {
            mapEditorMouseLookActive_ = false;
            syncInputMode();
            needsRedraw_ = true;
        }

        const util::Vec3 orthoForward = util::cameraForwardVector(
            util::mapEditorOrthoYawRadians(),
            util::mapEditorOrthoPitchRadians());
        const util::Vec3 orthoForwardFlat = normalizeVec3({orthoForward.x, 0.0f, orthoForward.z});
        const util::Vec3 orthoRight = normalizeVec3({-orthoForwardFlat.z, 0.0f, orthoForwardFlat.x});

        util::Vec3 pan{};
        if (input.moveForwardHeld) {
            pan = addVec3(pan, orthoForwardFlat);
        }
        if (input.moveBackwardHeld) {
            pan = subtractVec3(pan, orthoForwardFlat);
        }
        if (input.strafeLeftHeld) {
            pan = subtractVec3(pan, orthoRight);
        }
        if (input.strafeRightHeld) {
            pan = addVec3(pan, orthoRight);
        }

        const float panSpeed = std::max(6.0f, mapEditorOrthoSpan_ * 0.9f);
        if (lengthSquaredVec3(pan) > 0.0001f) {
            mapEditorOrthoFocusPosition_ = clampMapEditorOrthoFocusPosition(
                addVec3(
                    mapEditorOrthoFocusPosition_,
                    multiplyVec3(normalizeVec3(pan), panSpeed * deltaSeconds)));
            needsRedraw_ = true;
        }

        float nextSpan = mapEditorOrthoSpan_;
        if (input.moveUpHeld) {
            nextSpan += deltaSeconds * std::max(4.0f, mapEditorOrthoSpan_ * 1.15f);
        }
        if (input.moveDownHeld) {
            nextSpan -= deltaSeconds * std::max(4.0f, mapEditorOrthoSpan_ * 1.15f);
        }
        nextSpan = std::clamp(nextSpan, 4.0f, 96.0f);
        if (std::abs(nextSpan - mapEditorOrthoSpan_) > 0.001f) {
            mapEditorOrthoSpan_ = nextSpan;
            needsRedraw_ = true;
        }

        if (input.mouseWheelDelta != 0) {
            const float zoomScale = std::max(0.6f, mapEditorOrthoSpan_ * 0.12f);
            const float wheelZoom = static_cast<float>(input.mouseWheelDelta) * zoomScale;
            const float zoomedSpan = std::clamp(mapEditorOrthoSpan_ - wheelZoom, 4.0f, 96.0f);
            if (std::abs(zoomedSpan - mapEditorOrthoSpan_) > 0.001f) {
                mapEditorOrthoSpan_ = zoomedSpan;
                needsRedraw_ = true;
            }
        }
    }

    syncMapEditorCameraState();
    syncMapEditorTargetFromView();
}

void Application::updateSinglePlayerView(const platform::InputSnapshot& input, const float deltaSeconds) {
    constexpr float turnSpeed = 1.8f;
    constexpr float moveSpeed = 4.35f;
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    const content::WeaponDefinition fallbackWeapon{};
    const content::WeaponDefinition& activeWeapon = weapon != nullptr ? *weapon : fallbackWeapon;
    const WeaponHandlingProfile handling = makeHandlingProfile(activeWeapon);
    const float baseSpread = trainingBaseSpread(activeWeapon);
    const float mouseSensitivity = std::clamp(settings_.gameplay.mouseSensitivity, 0.05f, 3.0f);
    const float mouseVerticalSensitivity = std::clamp(settings_.gameplay.mouseVerticalSensitivity, 0.5f, 3.0f);
    const float maxPitchRadians = degreesToRadians(std::clamp(settings_.gameplay.maxLookPitchDegrees, 45.0f, 88.0f));
    const float mouseYawScale = 0.0026f * mouseSensitivity;
    const float mousePitchScale = 0.0026f * mouseSensitivity * mouseVerticalSensitivity;

    singlePlayerCameraYawRadians_ += static_cast<float>(input.mouseDeltaX) * mouseYawScale;
    singlePlayerCameraPitchRadians_ = std::clamp(
        singlePlayerCameraPitchRadians_ - static_cast<float>(input.mouseDeltaY) * mousePitchScale,
        -maxPitchRadians,
        maxPitchRadians);

    if (input.turnLeftHeld) {
        singlePlayerCameraYawRadians_ -= turnSpeed * deltaSeconds;
    }
    if (input.turnRightHeld) {
        singlePlayerCameraYawRadians_ += turnSpeed * deltaSeconds;
    }
    fireCooldownSeconds_ = std::max(0.0f, fireCooldownSeconds_ - deltaSeconds);
    hitFlashSeconds_ = std::max(0.0f, hitFlashSeconds_ - deltaSeconds);
    muzzleFlashSeconds_ = std::max(0.0f, muzzleFlashSeconds_ - deltaSeconds);
    flashOverlaySeconds_ = std::max(0.0f, flashOverlaySeconds_ - deltaSeconds);
    smokeOverlaySeconds_ = std::max(0.0f, smokeOverlaySeconds_ - deltaSeconds);
    aimYawOffsetRadians_ = approachZero(aimYawOffsetRadians_,
        degreesToRadians(activeWeapon.recoilYaw) * handling.recoilReturnSpeed * deltaSeconds);
    viewKickAmount_ = std::max(0.0f, viewKickAmount_ - handling.viewKickScale * 3.1f * deltaSeconds);
    crosshairSpreadDegrees_ = std::max(baseSpread, crosshairSpreadDegrees_ - handling.spreadRecovery * deltaSeconds);

    const float forwardX = std::cos(singlePlayerCameraYawRadians_);
    const float forwardZ = std::sin(singlePlayerCameraYawRadians_);
    const float rightX = -forwardZ;
    const float rightZ = forwardX;

    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (input.moveForwardHeld) {
        moveX += forwardX;
        moveZ += forwardZ;
    }
    if (input.moveBackwardHeld) {
        moveX -= forwardX;
        moveZ -= forwardZ;
    }
    if (input.strafeRightHeld) {
        moveX += rightX;
        moveZ += rightZ;
    }
    if (input.strafeLeftHeld) {
        moveX -= rightX;
        moveZ -= rightZ;
    }

    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    util::Vec3 desiredVelocity{};
    if (length > 0.0001f) {
        moveX /= length;
        moveZ /= length;
        desiredVelocity = {moveX * moveSpeed, 0.0f, moveZ * moveSpeed};
    }

    physicsWorld_.setLocalPlayerDesiredVelocity(desiredVelocity);
    physicsWorld_.step(deltaSeconds);
    handleMovementFeedback(baseSpread, handling.maxSpread);

    const util::Vec3 localVelocity = physicsWorld_.localPlayerLinearVelocity();
    const float horizontalSpeed = std::sqrt(localVelocity.x * localVelocity.x + localVelocity.z * localVelocity.z);
    const bool moving = horizontalSpeed > 0.15f;
    const bool airborne = !physicsWorld_.localPlayerSupported();
    if (moving) {
        crosshairSpreadDegrees_ = std::min(handling.maxSpread, crosshairSpreadDegrees_ + handling.moveSpreadGain * deltaSeconds);
    }
    if (airborne) {
        crosshairSpreadDegrees_ = std::min(handling.maxSpread, crosshairSpreadDegrees_ + handling.moveSpreadGain * 1.25f * deltaSeconds);
    }

    const util::Vec3 feetPosition = physicsWorld_.localPlayerFeetPosition();
    singlePlayerCameraPosition_ = {
        feetPosition.x,
        feetPosition.y + kSinglePlayerEyeHeight,
        feetPosition.z,
    };

    handlePendingDetonations();
}

gameplay::PlayerState Application::buildNetworkLocalPlayerState() const {
    gameplay::PlayerState playerState;
    playerState.id = networkSession_.localPlayerId();
    playerState.displayName = settings_.network.playerName;
    playerState.team = multiplayerSessionActive_ && multiplayerSessionType_ == network::SessionType::Client
        ? gameplay::Team::Defenders
        : gameplay::Team::Attackers;
    playerState.position = singlePlayerCameraPosition_;
    playerState.velocity = physicsWorld_.isReady() ? physicsWorld_.localPlayerLinearVelocity() : util::Vec3{};
    playerState.health = 100.0f;
    playerState.botControlled = false;
    playerState.loadout.primaryWeaponId = trainingWeaponIds_.empty()
        ? std::string("ak12")
        : trainingWeaponIds_[std::min(activeTrainingWeaponIndex_, trainingWeaponIds_.size() - 1)];
    playerState.loadout.secondaryWeaponId = "combat_knife";
    playerState.loadout.tacticalGrenadeId = "flashbang";
    playerState.loadout.lethalGrenadeId = selectedThrowableIndex_ % 3 == 2 ? "smoke" : "frag";
    playerState.loadout.optic = activeOptic_;
    return playerState;
}

void Application::syncLocalPlayerSimulationState() {
    if (!physicsWorld_.isReady()) {
        return;
    }

    const gameplay::Entity localPlayer = simulation_.firstPlayerEntity();
    if (localPlayer == gameplay::kNullEntity) {
        return;
    }

    const util::Vec3 feetPosition = physicsWorld_.localPlayerFeetPosition();
    const util::Vec3 linearVelocity = physicsWorld_.localPlayerLinearVelocity();
    simulation_.withPlayer(localPlayer, [&](gameplay::PlayerComponents& player) {
        player.transform.position = {
            feetPosition.x,
            feetPosition.y + kSinglePlayerEyeHeight,
            feetPosition.z,
        };
        player.velocity.linear = linearVelocity;
    });
}

void Application::applyLatestNetworkMapState() {
    if (!isRemoteClientSession()) {
        return;
    }

    const network::MapSyncState mapState = networkSession_.latestMapState();
    if (mapState.revision == 0 || mapState.revision == appliedNetworkMapRevision_) {
        return;
    }

    if (mapState.mapContent.empty()) {
        multiplayerStatus_ = "已收到主机地图消息，但内容为空。";
        return;
    }

    gameplay::MapData syncedMap = gameplay::MapSerializer::deserialize(mapState.mapContent);
    contentDatabase_.resolveMapData(syncedMap);
    const bool mapLooksValid = syncedMap.width > 0 && syncedMap.depth > 0 &&
        (!syncedMap.spawns.empty() || !syncedMap.props.empty() || !syncedMap.name.empty());
    if (!mapLooksValid) {
        multiplayerStatus_ = "主机地图解析失败，请确认两端版本一致。";
        spdlog::warn("[Network] Failed to deserialize host map: revision={} source={}",
            mapState.revision,
            mapState.sourceLabel);
        return;
    }

    activeMap_ = std::move(syncedMap);
    appliedNetworkMapRevision_ = mapState.revision;
    receivedMultiplayerSnapshot_ = false;
    initializeSinglePlayerView();

    simulation_ = gameplay::SimulationWorld(activeMap_);
    simulation_.setRules(gameplay::MatchRules{
        .mode = gameplay::MatchMode::BombDefusal,
        .roundTimeSeconds = 115,
        .buyTimeSeconds = 20,
        .maxRounds = 24,
        .friendlyFire = false,
    });
    simulation_.addPlayer(buildNetworkLocalPlayerState());
    syncLocalPlayerSimulationState();
    networkSession_.setLocalPlayerState(buildNetworkLocalPlayerState());

    const std::string hostMapLabel = mapState.mapName.empty() ? activeMap_.name : mapState.mapName;
    multiplayerStatus_ = "已同步主机地图: " + hostMapLabel + "，等待主机快照...";
    spdlog::info("[Network] 已同步主机地图: source={} name={} revision={}",
        mapState.sourceLabel,
        hostMapLabel,
        mapState.revision);
}

void Application::applyLatestNetworkSnapshot() {
    if (!isRemoteClientSession()) {
        return;
    }

    const network::SessionSnapshot snapshot = networkSession_.latestSnapshot();
    if (snapshot.players.empty()) {
        if (!receivedMultiplayerSnapshot_) {
            multiplayerStatus_ = "ENet 客户端已进入训练场，等待主机快照...";
        }
        return;
    }

    simulation_.replacePlayers(snapshot.players);
    simulation_.setElapsedSeconds(snapshot.elapsedSeconds);
    receivedMultiplayerSnapshot_ = true;
    multiplayerStatus_ = "已接收主机快照，当前同步玩家数: " + std::to_string(snapshot.players.size());
}

bool Application::isAuthoritativeGameplaySession() const {
    return !multiplayerSessionActive_ || multiplayerSessionType_ == network::SessionType::Host;
}

bool Application::isRemoteClientSession() const {
    return multiplayerSessionActive_ && multiplayerSessionType_ == network::SessionType::Client;
}

void Application::handleMovementFeedback(const float baseSpread, const float maxSpread) {
    for (const gameplay::PhysicsMovementEvent& event : physicsWorld_.consumeMovementEvents()) {
        switch (event.type) {
            case gameplay::PhysicsMovementEventType::Jumped:
                audioSystem_.play(audio::AudioCue::Jump);
                crosshairSpreadDegrees_ = std::min(maxSpread, crosshairSpreadDegrees_ + 0.24f);
                break;
            case gameplay::PhysicsMovementEventType::Landed:
                audioSystem_.play(audio::AudioCue::Landing);
                crosshairSpreadDegrees_ = std::min(maxSpread,
                    std::max(baseSpread, crosshairSpreadDegrees_) + 0.35f + event.intensity * 0.65f);
                viewKickAmount_ = std::min(1.75f, viewKickAmount_ + 0.12f + event.intensity * 0.26f);
                break;
            case gameplay::PhysicsMovementEventType::SteppedUp:
                audioSystem_.play(audio::AudioCue::Footstep);
                if (event.height > 0.18f) {
                    viewKickAmount_ = std::min(1.75f, viewKickAmount_ + 0.04f + event.intensity * 0.06f);
                }
                break;
        }
    }
}

void Application::handlePendingDetonations() {
    for (const gameplay::PhysicsDetonationEvent& detonation : physicsWorld_.consumeDetonations()) {
        if (detonation.itemId == "frag") {
            int hitCount = 0;
            simulation_.forEachPlayer([&](gameplay::PlayerComponents& player) {
                if (!player.botControlled) {
                    return;
                }

                const float dx = player.transform.position.x - detonation.position.x;
                const float dy = player.transform.position.y - detonation.position.y;
                const float dz = player.transform.position.z - detonation.position.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > detonation.effectRadius || lineOfSightBlocked(detonation.position, player.transform.position)) {
                    return;
                }

                player.health.current -= std::max(24.0f, 120.0f - distance * 22.0f);
                ++hitCount;
                if (player.health.current <= 0.0f) {
                    ++eliminations_;
                    player.health.current = player.health.maximum;
                    player.transform.position = {18.5f, 1.0f, 18.5f};
                    player.velocity.linear = {-0.15f, 0.0f, 0.0f};
                }
            });
            hitFlashSeconds_ = hitCount > 0 ? 0.14f : hitFlashSeconds_;
            muzzleFlashSeconds_ = std::max(muzzleFlashSeconds_, 0.10f);
            audioSystem_.play(audio::AudioCue::FragExplosion);
            spdlog::info("[SinglePlayer] 破片手雷起爆，命中目标数: {}.", hitCount);
            continue;
        }

        if (detonation.itemId == "flashbang") {
            const float dx = singlePlayerCameraPosition_.x - detonation.position.x;
            const float dy = singlePlayerCameraPosition_.y - detonation.position.y;
            const float dz = singlePlayerCameraPosition_.z - detonation.position.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance <= detonation.effectRadius && !lineOfSightBlocked(detonation.position, singlePlayerCameraPosition_)) {
                flashOverlaySeconds_ = std::max(flashOverlaySeconds_, 1.1f);
            }
            audioSystem_.play(audio::AudioCue::FlashBurst);
            spdlog::info("[SinglePlayer] 闪光弹起爆。");
            continue;
        }

        if (detonation.itemId == "smoke") {
            const float dx = singlePlayerCameraPosition_.x - detonation.position.x;
            const float dy = singlePlayerCameraPosition_.y - detonation.position.y;
            const float dz = singlePlayerCameraPosition_.z - detonation.position.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance <= detonation.effectRadius * 1.2f) {
                smokeOverlaySeconds_ = std::max(smokeOverlaySeconds_, 4.5f);
            }
            audioSystem_.play(audio::AudioCue::SmokeBurst);
            spdlog::info("[SinglePlayer] 烟雾弹起爆。");
        }
    }
}

bool Application::collidesWithWorld(const util::Vec3& position) const {
    const float radius = 0.20f;
    const auto sampleSolid = [this, &position](const float x, const float z) {
        if (x < 0.0f || z < 0.0f || x >= static_cast<float>(activeMap_.width) || z >= static_cast<float>(activeMap_.depth)) {
            return true;
        }

        const util::Vec3 samplePosition{x, position.y, z};
        for (const auto& prop : activeMap_.props) {
            if (pointInsidePropVolume(prop, samplePosition)) {
                return true;
            }
        }

        return false;
    };

    return sampleSolid(position.x - radius, position.z - radius) ||
           sampleSolid(position.x + radius, position.z - radius) ||
           sampleSolid(position.x - radius, position.z + radius) ||
           sampleSolid(position.x + radius, position.z + radius);
}

const content::WeaponDefinition* Application::findWeaponDefinition(const std::string& id) const {
    for (const auto& weapon : contentDatabase_.weapons()) {
        if (weapon.id == id) {
            return &weapon;
        }
    }
    return nullptr;
}

void Application::switchToNextTrainingWeapon() {
    if (trainingWeaponIds_.empty()) {
        return;
    }

    activeTrainingWeaponIndex_ = (activeTrainingWeaponIndex_ + 1) % trainingWeaponIds_.size();
    if (const auto* weapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_])) {
        activeWeaponLabel_ = weapon->displayName;
        ammoInMagazine_ = weapon->magazineSize;
        reserveAmmo_ = weapon->reserveAmmo;
        fireCooldownSeconds_ = 0.0f;
        hitFlashSeconds_ = 0.0f;
        muzzleFlashSeconds_ = 0.0f;
        aimYawOffsetRadians_ = 0.0f;
        viewKickAmount_ = 0.0f;
        crosshairSpreadDegrees_ = trainingBaseSpread(*weapon);
        spdlog::info("[SinglePlayer] Switched weapon to {}.", weapon->displayName);
    }
}

void Application::selectTrainingEquipmentSlot(const TrainingEquipmentSlot slot) {
    if (activeTrainingSlot_ == slot) {
        return;
    }
    activeTrainingSlot_ = slot;
    needsRedraw_ = true;
}

void Application::cycleTrainingThrowable() {
    selectedThrowableIndex_ = (selectedThrowableIndex_ + 1) % 3;
    activeTrainingSlot_ = TrainingEquipmentSlot::Throwable;
    needsRedraw_ = true;
    spdlog::info("[SinglePlayer] 当前投掷物: {}.", throwableLabel(selectedThrowableIndex_));
}

void Application::cycleTrainingOptic() {
    switch (activeOptic_) {
        case content::OpticType::IronSight:
            activeOptic_ = content::OpticType::RedDot;
            break;
        case content::OpticType::RedDot:
            activeOptic_ = content::OpticType::X2;
            break;
        case content::OpticType::X2:
            activeOptic_ = content::OpticType::X4;
            break;
        case content::OpticType::X4:
            activeOptic_ = content::OpticType::X8;
            break;
        case content::OpticType::X8:
            activeOptic_ = content::OpticType::RedDot;
            break;
    }
    needsRedraw_ = true;
    spdlog::info("[SinglePlayer] 当前准镜: {}.", opticLabel(activeOptic_));
}

void Application::reloadTrainingWeapon() {
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    if (weapon == nullptr || reserveAmmo_ <= 0 || ammoInMagazine_ >= weapon->magazineSize) {
        return;
    }

    const int missing = weapon->magazineSize - ammoInMagazine_;
    const int moved = std::min(missing, reserveAmmo_);
    ammoInMagazine_ += moved;
    reserveAmmo_ -= moved;
    audioSystem_.play(audio::AudioCue::Reload);
    needsRedraw_ = true;
}

void Application::fireTrainingWeapon() {
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    if (weapon == nullptr || fireCooldownSeconds_ > 0.0f) {
        return;
    }
    const WeaponHandlingProfile handling = makeHandlingProfile(*weapon);

    if (ammoInMagazine_ <= 0) {
        if (!settings_.gameplay.autoReload) {
            spdlog::info("[SinglePlayer] 弹匣已空，按 R 换弹。");
            return;
        }
        reloadTrainingWeapon();
        return;
    }

    --ammoInMagazine_;
    fireCooldownSeconds_ = 60.0f / std::max(1.0f, weapon->fireRateRpm);
    muzzleFlashSeconds_ = handling.muzzleFlashDuration;
    audioSystem_.play(audio::AudioCue::WeaponShot);
    needsRedraw_ = true;

    const util::Vec3 localVelocity = physicsWorld_.localPlayerLinearVelocity();
    const bool moving = std::sqrt(localVelocity.x * localVelocity.x + localVelocity.z * localVelocity.z) > 0.15f;
    const bool airborne = !physicsWorld_.localPlayerSupported();
    const float baseSpread = trainingBaseSpread(*weapon);
    const float movementPenalty = moving ? weapon->hipSpread * 0.28f : 0.0f;
    const float airbornePenalty = airborne ? 1.1f : 0.0f;
    const float shotSpreadDegrees = std::min(handling.maxSpread,
        std::max(baseSpread, crosshairSpreadDegrees_) + movementPenalty + airbornePenalty);
    const float shotYaw = singlePlayerCameraYawRadians_ + aimYawOffsetRadians_ +
        degreesToRadians(randomSigned(recoilRandom_) * shotSpreadDegrees * 0.42f);
    const float shotPitch = singlePlayerCameraPitchRadians_ +
        degreesToRadians(randomSigned(recoilRandom_) * shotSpreadDegrees * 0.12f);
    const float forwardX = std::cos(shotYaw);
    const float forwardZ = std::sin(shotYaw);
    util::Vec3 origin = singlePlayerCameraPosition_;

    float bestDistance = std::numeric_limits<float>::max();
    gameplay::Entity bestTarget = gameplay::kNullEntity;
    simulation_.forEachPlayer([&](gameplay::PlayerComponents& player) {
        if (!player.botControlled) {
            return;
        }

        util::Vec3 toTarget{
            player.transform.position.x - origin.x,
            0.0f,
            player.transform.position.z - origin.z,
        };
        const float forwardDistance = toTarget.x * forwardX + toTarget.z * forwardZ;
        if (forwardDistance < 0.35f || forwardDistance > handling.effectiveRange) {
            return;
        }

        const float lateralDistance = std::abs(toTarget.x * -forwardZ + toTarget.z * forwardX);
        if (lateralDistance > handling.targetRadius) {
            return;
        }
        const float shotHeightAtTarget = origin.y + std::tan(shotPitch) * forwardDistance;
        const float verticalTolerance = 0.85f + handling.targetRadius * 0.25f;
        if (std::abs(player.transform.position.y - shotHeightAtTarget) > verticalTolerance) {
            return;
        }
        if (lineOfSightBlocked(origin, player.transform.position)) {
            return;
        }
        if (forwardDistance < bestDistance) {
            bestDistance = forwardDistance;
            bestTarget = player.entity;
        }
    });

    crosshairSpreadDegrees_ = std::min(handling.maxSpread,
        std::max(baseSpread, crosshairSpreadDegrees_) + handling.shotSpreadKick + movementPenalty * 0.55f + airbornePenalty);
    aimYawOffsetRadians_ += degreesToRadians(weapon->recoilYaw * handling.yawKickScale * randomSigned(recoilRandom_));
    const float maxYawKick = degreesToRadians(std::max(0.4f, weapon->recoilYaw * 2.8f));
    aimYawOffsetRadians_ = std::clamp(aimYawOffsetRadians_, -maxYawKick, maxYawKick);
    viewKickAmount_ = std::min(1.75f, viewKickAmount_ + weapon->recoilPitch * 0.085f * handling.viewKickScale);

    if (bestTarget == gameplay::kNullEntity) {
        return;
    }

    bool eliminated = false;
    simulation_.withPlayer(bestTarget, [&](gameplay::PlayerComponents& player) {
        player.health.current -= weapon->damage;
        eliminated = player.health.current <= 0.0f;
    });
    hitFlashSeconds_ = 0.14f;
    audioSystem_.play(audio::AudioCue::HitConfirm);
    crosshairSpreadDegrees_ = std::max(baseSpread, crosshairSpreadDegrees_ - handling.shotSpreadKick * 0.18f);
    if (eliminated) {
        ++eliminations_;
        simulation_.respawnPlayer(bestTarget, {18.5f, 1.0f, 18.5f}, {-0.15f, 0.0f, 0.0f});
        spdlog::info("[SinglePlayer] Target eliminated. Total eliminations: {}", eliminations_);
    }
}

void Application::useTrainingThrowable() {
    auto consumeThrowable = [](int& count) {
        if (count <= 0) {
            return false;
        }
        --count;
        return true;
    };

    const float cosPitch = std::cos(singlePlayerCameraPitchRadians_);
    const util::Vec3 forward{
        std::cos(singlePlayerCameraYawRadians_) * cosPitch,
        std::sin(singlePlayerCameraPitchRadians_),
        std::sin(singlePlayerCameraYawRadians_) * cosPitch,
    };
    const util::Vec3 spawnPosition{
        singlePlayerCameraPosition_.x + forward.x * 0.55f,
        singlePlayerCameraPosition_.y + forward.y * 0.55f - 0.12f,
        singlePlayerCameraPosition_.z + forward.z * 0.55f,
    };
    const util::Vec3 throwVelocity{
        forward.x * 10.5f,
        forward.y * 10.5f + 1.2f,
        forward.z * 10.5f,
    };

    switch (selectedThrowableIndex_ % 3) {
        case 0: {
            if (!consumeThrowable(fragCount_)) {
                return;
            }
            physicsWorld_.spawnThrowable("frag", spawnPosition, throwVelocity, 1.8f, 4.2f);
            spdlog::info("[SinglePlayer] 投掷破片手雷。");
            break;
        }
        case 1:
            if (!consumeThrowable(flashCount_)) {
                return;
            }
            physicsWorld_.spawnThrowable("flashbang", spawnPosition, throwVelocity, 1.2f, 8.0f);
            spdlog::info("[SinglePlayer] 投掷闪光弹。");
            break;
        default:
            if (!consumeThrowable(smokeCount_)) {
                return;
            }
            physicsWorld_.spawnThrowable("smoke", spawnPosition, throwVelocity, 1.5f, 6.0f);
            spdlog::info("[SinglePlayer] 投掷烟雾弹。");
            break;
    }

    audioSystem_.play(audio::AudioCue::ThrowableThrow);
    needsRedraw_ = true;
}

void Application::useTrainingMelee() {
    const float forwardX = std::cos(singlePlayerCameraYawRadians_);
    const float forwardZ = std::sin(singlePlayerCameraYawRadians_);
    gameplay::Entity bestTarget = gameplay::kNullEntity;
    float bestDistance = 1.65f;

    simulation_.forEachPlayer([&](gameplay::PlayerComponents& player) {
        if (!player.botControlled) {
            return;
        }
        const float dx = player.transform.position.x - singlePlayerCameraPosition_.x;
        const float dz = player.transform.position.z - singlePlayerCameraPosition_.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > bestDistance) {
            return;
        }
        const float facing = (dx * forwardX + dz * forwardZ) / std::max(0.001f, distance);
        if (facing < 0.55f || lineOfSightBlocked(singlePlayerCameraPosition_, player.transform.position)) {
            return;
        }
        bestDistance = distance;
        bestTarget = player.entity;
    });

    muzzleFlashSeconds_ = 0.0f;
    audioSystem_.play(audio::AudioCue::MeleeSwing);
    if (bestTarget == gameplay::kNullEntity) {
        needsRedraw_ = true;
        return;
    }

    bool eliminated = false;
    simulation_.withPlayer(bestTarget, [&](gameplay::PlayerComponents& player) {
        player.health.current -= 55.0f;
        eliminated = player.health.current <= 0.0f;
    });
    hitFlashSeconds_ = 0.10f;
    if (eliminated) {
        ++eliminations_;
        simulation_.respawnPlayer(bestTarget, {18.5f, 1.0f, 18.5f}, {-0.15f, 0.0f, 0.0f});
    }
    needsRedraw_ = true;
}

void Application::handleMapEditorInput(const platform::InputSnapshot& input) {
    if (input.editorUndoPressed) {
        if (!restoreMapEditorUndoSnapshot()) {
            mapEditorStatus_ = "当前没有可撤销的操作";
            needsRedraw_ = true;
        }
        return;
    }

    if (input.editorToggleProjectionPressed) {
        switchMapEditorViewMode(
            mapEditorViewMode_ == MapEditorViewMode::Perspective
                ? MapEditorViewMode::Ortho25D
                : MapEditorViewMode::Perspective);
    }

    if (input.selectPrimaryPressed) {
        mapEditorTool_ = MapEditorTool::Select;
        mapEditorStatus_ = "已切换工具: 选择";
        syncMapEditorTargetFromView();
        needsRedraw_ = true;
    }
    if (input.selectSecondaryPressed) {
        mapEditorTool_ = MapEditorTool::Pan;
        mapEditorStatus_ = "已切换工具: 抓手";
        needsRedraw_ = true;
    }
    if (input.selectMeleePressed) {
        mapEditorTool_ = MapEditorTool::Place;
        mapEditorStatus_ = std::string("已切换工具: 放置 / ") + mapEditorPlacementKindLabel();
        needsRedraw_ = true;
    }
    if (input.selectThrowablePressed) {
        mapEditorTool_ = MapEditorTool::Erase;
        mapEditorStatus_ = "已切换工具: 擦除";
        needsRedraw_ = true;
    }
    if (input.selectToolFivePressed) {
        mapEditorPlacementKind_ = MapEditorPlacementKind::AttackerSpawn;
        mapEditorTool_ = MapEditorTool::Place;
        mapEditorStatus_ = std::string("放置类型: ") + mapEditorPlacementKindLabel();
        needsRedraw_ = true;
    }

    if (input.cycleThrowablePressed) {
        cycleMapEditorPlacementKind(1);
    }

    if (input.reloadPressed) {
        if (gameplay::MapProp* prop = selectedMapEditorProp(); prop != nullptr) {
            pushMapEditorUndoSnapshot("快捷旋转道具");
            prop->rotationDegrees.y = wrapDegrees(prop->rotationDegrees.y + 15.0f);
            mapEditorStatus_ = std::string("已旋转选中对象: ") + propDisplayLabel(*prop);
            syncMapEditorTargetFromView();
        } else if (mapEditorTool_ == MapEditorTool::Place &&
                   (mapEditorPlacementKind_ == MapEditorPlacementKind::Wall ||
                    mapEditorPlacementKind_ == MapEditorPlacementKind::Prop)) {
            editorPropRotationDegrees_ = std::fmod(editorPropRotationDegrees_ + 45.0f, 360.0f);
            mapEditorStatus_ = std::string("放置旋转预设: ")
                + std::to_string(static_cast<int>(std::lround(editorPropRotationDegrees_)))
                + "°";
        }
        needsRedraw_ = true;
    }
    if (input.switchWeaponPressed) {
        if (gameplay::MapProp* prop = selectedMapEditorProp(); prop != nullptr) {
            editorPropScalePresetIndex_ = (editorPropScalePresetIndex_ + 1) % kEditorPropScalePresets.size();
            const auto& scalePreset = editorPropScalePreset(editorPropScalePresetIndex_);
            pushMapEditorUndoSnapshot("快捷缩放道具");
            prop->scale = {scalePreset.uniformScale, scalePreset.uniformScale, scalePreset.uniformScale};
            mapEditorStatus_ = std::string("已调整选中对象缩放: ") + scalePreset.label;
            syncMapEditorTargetFromView();
        } else if (mapEditorTool_ == MapEditorTool::Place &&
                   (mapEditorPlacementKind_ == MapEditorPlacementKind::Wall ||
                    mapEditorPlacementKind_ == MapEditorPlacementKind::Prop)) {
            editorPropScalePresetIndex_ = (editorPropScalePresetIndex_ + 1) % kEditorPropScalePresets.size();
            mapEditorStatus_ = std::string("放置缩放预设: ") + editorPropScalePreset(editorPropScalePresetIndex_).label;
        }
        needsRedraw_ = true;
    }

    if (input.editorDeletePressed) {
        eraseMapEditorCell();
        return;
    }
    if (input.editorPreviousMapPressed) {
        cycleEditorMap(-1);
        return;
    }
    if (input.editorNextMapPressed) {
        cycleEditorMap(1);
        return;
    }
    if (input.editorNewMapPressed) {
        createNewEditorMap();
        return;
    }
    if (input.editorSavePressed) {
        saveActiveMapArtifacts("编辑器保存");
        mapEditorStatus_ = "地图与预览图已保存";
        needsRedraw_ = true;
        return;
    }

    if (input.primaryClickPressed && !input.secondaryClickHeld) {
        if (mapEditorTool_ == MapEditorTool::Select && mapEditorViewMode_ == MapEditorViewMode::Ortho25D) {
            applyMapEditorTool();
        } else if (mapEditorTool_ != MapEditorTool::Select && mapEditorTool_ != MapEditorTool::Pan) {
            applyMapEditorTool();
        }
        return;
    }
    if (input.confirmPressed && !input.jumpPressed &&
        mapEditorTool_ != MapEditorTool::Select &&
        mapEditorTool_ != MapEditorTool::Pan) {
        applyMapEditorTool();
    }
}

std::optional<std::size_t> Application::pickMapEditorPropFromView(float* outDistance) const {
    util::Vec3 origin{};
    util::Vec3 direction{};
    if (!buildMapEditorRay(origin, direction)) {
        if (outDistance != nullptr) {
            *outDistance = std::numeric_limits<float>::max();
        }
        return std::nullopt;
    }
    return pickMapEditorPropFromRay(activeMap_, origin, direction, kMapEditorMaxPlacementDistance, outDistance);
}

std::optional<std::size_t> Application::pickMapEditorSpawnFromView(float* outDistance) const {
    util::Vec3 origin{};
    util::Vec3 direction{};
    if (!buildMapEditorRay(origin, direction)) {
        if (outDistance != nullptr) {
            *outDistance = std::numeric_limits<float>::max();
        }
        return std::nullopt;
    }
    return pickMapEditorSpawnFromRay(
        activeMap_,
        origin,
        direction,
        kMapEditorMaxPlacementDistance,
        spawnSelectionRadius(),
        outDistance);
}

bool Application::pickMapEditorCellFromView(int& cellX, int& cellZ, float* outDistance) const {
    const util::Vec3 origin = mapEditorCameraPosition_;
    const util::Vec3 direction = util::cameraForwardVector(mapEditorCameraYawRadians_, mapEditorCameraPitchRadians_);
    constexpr float kMaxDistance = 128.0f;
    constexpr float kStep = 0.20f;
    float bestDistance = std::numeric_limits<float>::max();

    for (float distance = 0.4f; distance <= kMaxDistance; distance += kStep) {
        const util::Vec3 point = addVec3(origin, multiplyVec3(direction, distance));
        if (point.x < 0.0f || point.z < 0.0f ||
            point.x >= static_cast<float>(activeMap_.width) ||
            point.z >= static_cast<float>(activeMap_.depth)) {
            continue;
        }

        const float floorHeight = gameplay::sampleFloorHeight(activeMap_, point.x, point.z);
        if (point.y > floorHeight + 0.20f) {
            continue;
        }

        cellX = std::clamp(static_cast<int>(std::floor(point.x)), 0, std::max(0, activeMap_.width - 1));
        cellZ = std::clamp(static_cast<int>(std::floor(point.z)), 0, std::max(0, activeMap_.depth - 1));
        bestDistance = distance;
        if (outDistance != nullptr) {
            *outDistance = bestDistance;
        }
        return true;
    }

    if (std::abs(direction.y) > 0.0001f) {
        const float planeDistance = -origin.y / direction.y;
        if (planeDistance > 0.0f) {
            const util::Vec3 point = addVec3(origin, multiplyVec3(direction, planeDistance));
            if (point.x >= 0.0f && point.z >= 0.0f &&
                point.x < static_cast<float>(activeMap_.width) &&
                point.z < static_cast<float>(activeMap_.depth)) {
                cellX = std::clamp(static_cast<int>(std::floor(point.x)), 0, std::max(0, activeMap_.width - 1));
                cellZ = std::clamp(static_cast<int>(std::floor(point.z)), 0, std::max(0, activeMap_.depth - 1));
                if (outDistance != nullptr) {
                    *outDistance = planeDistance;
                }
                return true;
            }
        }
    }

    return false;
}

void Application::syncMapEditorTargetFromView() {
    util::Vec3 origin{};
    util::Vec3 direction{};
    if (!buildMapEditorRay(origin, direction)) {
        hoveredEditorPropIndex_.reset();
        hoveredEditorSpawnIndex_.reset();
        return;
    }

    RaySurfaceHit propHit{};
    float propDistance = std::numeric_limits<float>::max();
    const std::optional<std::size_t> pickedPropIndex = pickMapEditorPropFromRay(
        activeMap_,
        origin,
        direction,
        kMapEditorMaxPlacementDistance,
        &propDistance,
        &propHit);
    float spawnDistance = std::numeric_limits<float>::max();
    const std::optional<std::size_t> pickedSpawnIndex = pickMapEditorSpawnFromRay(
        activeMap_,
        origin,
        direction,
        kMapEditorMaxPlacementDistance,
        spawnSelectionRadius(),
        &spawnDistance);

    hoveredEditorPropIndex_.reset();
    hoveredEditorSpawnIndex_.reset();
    if (pickedPropIndex.has_value() &&
        (!pickedSpawnIndex.has_value() || propDistance <= spawnDistance + 0.02f)) {
        hoveredEditorPropIndex_ = pickedPropIndex;
    } else if (pickedSpawnIndex.has_value()) {
        hoveredEditorSpawnIndex_ = pickedSpawnIndex;
    }

    RaySurfaceHit bestHit{};
    bool hasSurfaceHit = false;
    RaySurfaceHit groundHit{};
    if (rayIntersectGroundPlane(activeMap_, origin, direction, kMapEditorMaxPlacementDistance, groundHit)) {
        bestHit = groundHit;
        hasSurfaceHit = true;
    }
    if (pickedPropIndex.has_value() && (!hasSurfaceHit || propHit.distance < bestHit.distance)) {
        bestHit = propHit;
        hasSurfaceHit = true;
    }

    mapEditorHasTarget_ = true;
    mapEditorTargetOnSurface_ = hasSurfaceHit;
    if (hasSurfaceHit) {
        mapEditorTargetPosition_ = clampEditorTargetPosition(activeMap_, bestHit.point);
        mapEditorTargetNormal_ = normalizeVec3(bestHit.normal);
        if (lengthSquaredVec3(mapEditorTargetNormal_) <= 0.0001f) {
            mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
        }
    } else {
        mapEditorTargetPosition_ = defaultFloatingEditorTargetPosition(activeMap_, origin, direction);
        mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
    }

    mapEditorCursorX_ = std::clamp(static_cast<int>(std::floor(mapEditorTargetPosition_.x)), 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(static_cast<int>(std::floor(mapEditorTargetPosition_.z)), 0, std::max(0, activeMap_.depth - 1));
    if (mapEditorTool_ == MapEditorTool::Select && mapEditorViewMode_ == MapEditorViewMode::Perspective) {
        selectedEditorPropIndex_ = hoveredEditorPropIndex_;
    }
}

std::optional<std::size_t> Application::findMapEditorPropIndexAtCell(const int cellX, const int cellZ) const {
    if (activeMap_.props.empty()) {
        return std::nullopt;
    }

    const util::Vec3 cellCenter = centerOfCell(cellX, cellZ, 0.0f);
    std::optional<std::size_t> bestIndex;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < activeMap_.props.size(); ++index) {
        const gameplay::MapProp& prop = activeMap_.props[index];
        if (!positionInsideCell(prop.position, cellX, cellZ) &&
            !pointHitsPropFootprint(prop, cellCenter.x, cellCenter.z)) {
            continue;
        }

        const float dx = prop.position.x - cellCenter.x;
        const float dz = prop.position.z - cellCenter.z;
        const float distanceSquared = dx * dx + dz * dz;
        if (!bestIndex.has_value() || distanceSquared < bestDistanceSquared) {
            bestIndex = index;
            bestDistanceSquared = distanceSquared;
        }
    }

    return bestIndex;
}

void Application::syncSelectedMapEditorPropFromCursor() {
    selectedEditorPropIndex_ = findMapEditorPropIndexAtCell(mapEditorCursorX_, mapEditorCursorZ_);
}

gameplay::MapProp* Application::selectedMapEditorProp() {
    if (!selectedEditorPropIndex_.has_value() || *selectedEditorPropIndex_ >= activeMap_.props.size()) {
        selectedEditorPropIndex_.reset();
        return nullptr;
    }
    return &activeMap_.props[*selectedEditorPropIndex_];
}

const gameplay::MapProp* Application::selectedMapEditorProp() const {
    if (!selectedEditorPropIndex_.has_value() || *selectedEditorPropIndex_ >= activeMap_.props.size()) {
        return nullptr;
    }
    return &activeMap_.props[*selectedEditorPropIndex_];
}

bool Application::buildMapEditorRay(util::Vec3& origin, util::Vec3& direction) const {
    if (window_ == nullptr) {
        return false;
    }

    if (!mapEditorMouseLookActive_ &&
        renderer_ != nullptr &&
        renderer_->wantsMouseCapture()) {
        return false;
    }

    const int width = std::max(1, window_->clientWidth());
    const int height = std::max(1, window_->clientHeight());
    float sampleX = static_cast<float>(width) * 0.5f;
    float sampleY = static_cast<float>(height) * 0.5f;
    if (mapEditorViewMode_ == MapEditorViewMode::Ortho25D && !mapEditorMouseLookActive_) {
        sampleX = std::clamp(static_cast<float>(lastInput_.mouseX), 0.0f, static_cast<float>(width - 1)) + 0.5f;
        sampleY = std::clamp(static_cast<float>(lastInput_.mouseY), 0.0f, static_cast<float>(height - 1)) + 0.5f;
    }

    const glm::mat4 projection = util::buildMapEditorProjectionMatrix(
        mapEditorViewMode_ == MapEditorViewMode::Ortho25D,
        static_cast<float>(width),
        static_cast<float>(height),
        mapEditorOrthoSpan_);
    const glm::mat4 view = util::buildMapEditorViewMatrix(
        mapEditorViewMode_ == MapEditorViewMode::Ortho25D,
        mapEditorCameraPosition_,
        mapEditorCameraYawRadians_,
        mapEditorCameraPitchRadians_,
        mapEditorOrthoSpan_);
    const glm::mat4 inverseProjectionView = glm::inverse(projection * view);

    const float ndcX = sampleX / static_cast<float>(width) * 2.0f - 1.0f;
    const float ndcY = sampleY / static_cast<float>(height) * 2.0f - 1.0f;
    glm::vec4 nearPoint = inverseProjectionView * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    glm::vec4 farPoint = inverseProjectionView * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearPoint.w) <= 0.0001f || std::abs(farPoint.w) <= 0.0001f) {
        return false;
    }

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    const util::Vec3 nearWorld{nearPoint.x, nearPoint.y, nearPoint.z};
    const util::Vec3 farWorld{farPoint.x, farPoint.y, farPoint.z};
    direction = normalizeVec3(subtractVec3(farWorld, nearWorld));
    if (lengthSquaredVec3(direction) <= 0.0001f) {
        return false;
    }

    if (mapEditorViewMode_ == MapEditorViewMode::Ortho25D) {
        origin = nearWorld;
    } else {
        origin = mapEditorCameraPosition_;
    }
    return true;
}

gameplay::MapProp Application::buildMapEditorPlacementPreviewProp() const {
    const util::Vec3 targetPoint = mapEditorHasTarget_
        ? mapEditorTargetPosition_
        : defaultFloatingEditorTargetPosition(
            activeMap_,
            mapEditorCameraPosition_,
            util::cameraForwardVector(mapEditorCameraYawRadians_, mapEditorCameraPitchRadians_));
    const util::Vec3 targetNormal = mapEditorTargetOnSurface_
        ? mapEditorTargetNormal_
        : util::Vec3{0.0f, 1.0f, 0.0f};

    gameplay::MapProp prop{};
    const content::ObjectAssetDefinition* selectedObjectAsset = selectedMapEditorObjectAsset();
    if (mapEditorPlacementKind_ == MapEditorPlacementKind::Wall) {
        const auto& scalePreset = editorPropScalePreset(editorPropScalePresetIndex_);
        const std::string objectId =
            selectedObjectAsset != nullptr && selectedObjectAsset->placementKind == content::ObjectPlacementKind::Wall
                ? selectedObjectAsset->id
                : "editor_brush_wall";
        prop = contentDatabase_.instantiateMapProp(
            objectId,
            targetPoint,
            {0.0f, editorPropRotationDegrees_, 0.0f},
            {2.4f * scalePreset.uniformScale, 2.6f * scalePreset.uniformScale, 0.36f * scalePreset.uniformScale});
    } else {
        const auto& scalePreset = editorPropScalePreset(editorPropScalePresetIndex_);
        const std::string objectId =
            selectedObjectAsset != nullptr && selectedObjectAsset->placementKind == content::ObjectPlacementKind::Prop
                ? selectedObjectAsset->id
                : "wooden_crate";
        prop = contentDatabase_.instantiateMapProp(
            objectId,
            targetPoint,
            {0.0f, editorPropRotationDegrees_, 0.0f},
            {scalePreset.uniformScale, scalePreset.uniformScale, scalePreset.uniformScale});
    }

    prop.position = clampEditorTargetPosition(
        activeMap_,
        editorPlacementOriginFromTarget(prop, targetPoint, targetNormal, mapEditorTargetOnSurface_));
    return prop;
}

gameplay::SpawnPoint Application::buildMapEditorPlacementPreviewSpawn() const {
    const gameplay::Team team = mapEditorPlacementKind_ == MapEditorPlacementKind::DefenderSpawn
        ? gameplay::Team::Defenders
        : gameplay::Team::Attackers;
    const util::Vec3 targetPoint = mapEditorHasTarget_
        ? mapEditorTargetPosition_
        : defaultFloatingEditorTargetPosition(
            activeMap_,
            mapEditorCameraPosition_,
            util::cameraForwardVector(mapEditorCameraYawRadians_, mapEditorCameraPitchRadians_));
    return gameplay::SpawnPoint{
        team,
        clampEditorTargetPosition(activeMap_, {targetPoint.x, targetPoint.y + kSinglePlayerEyeHeight, targetPoint.z}),
    };
}

void Application::cycleMapEditorPlacementKind(const int delta) {
    const int count = static_cast<int>(MapEditorPlacementKind::DefenderSpawn) + 1;
    const int current = static_cast<int>(mapEditorPlacementKind_);
    mapEditorPlacementKind_ = static_cast<MapEditorPlacementKind>((current + delta + count) % count);
    clampMapEditorAssetSelection();
    mapEditorTool_ = MapEditorTool::Place;
    mapEditorStatus_ = std::string("放置类型: ") + mapEditorPlacementKindLabel();
    needsRedraw_ = true;
}

void Application::pushMapEditorUndoSnapshot(const char* reason) {
    if (mapEditorUndoStack_.size() >= kMapEditorUndoLimit) {
        mapEditorUndoStack_.erase(mapEditorUndoStack_.begin());
    }
    mapEditorUndoStack_.push_back(activeMap_);
    spdlog::info("[MapEditor] Undo snapshot saved: {} ({})", reason, mapEditorUndoStack_.size());
}

bool Application::restoreMapEditorUndoSnapshot() {
    if (mapEditorUndoStack_.empty()) {
        return false;
    }

    activeMap_ = mapEditorUndoStack_.back();
    mapEditorUndoStack_.pop_back();
    hoveredEditorPropIndex_.reset();
    hoveredEditorSpawnIndex_.reset();
    if (selectedEditorPropIndex_.has_value() && *selectedEditorPropIndex_ >= activeMap_.props.size()) {
        selectedEditorPropIndex_.reset();
    }
    mapEditorStatus_ = "已撤销上一步";
    syncMapEditorTargetFromView();
    needsRedraw_ = true;
    return true;
}

void Application::clearMapEditorUndoHistory() {
    mapEditorUndoStack_.clear();
}

void Application::clampMapEditorAssetSelection() {
    const auto& objectAssets = contentDatabase_.objectAssets();
    if (objectAssets.empty()) {
        editorObjectAssetIndex_ = 0;
        return;
    }
    if (editorObjectAssetIndex_ >= objectAssets.size()) {
        editorObjectAssetIndex_ = objectAssets.size() - 1;
    }
    if (selectedMapEditorObjectAsset() != nullptr) {
        return;
    }
    for (std::size_t index = 0; index < objectAssets.size(); ++index) {
        editorObjectAssetIndex_ = index;
        if (selectedMapEditorObjectAsset() != nullptr) {
            return;
        }
    }
}

void Application::clampManagedObjectAssetSelection() {
    const auto& objectAssets = contentDatabase_.objectAssets();
    if (objectAssets.empty()) {
        managedObjectAssetIndex_ = 0;
    } else if (managedObjectAssetIndex_ >= objectAssets.size()) {
        managedObjectAssetIndex_ = objectAssets.size() - 1;
    }
}

std::vector<const content::ObjectAssetDefinition*> Application::mapEditorSelectableObjects() const {
    std::vector<const content::ObjectAssetDefinition*> objects;
    const auto& allObjects = contentDatabase_.objectAssets();
    if (mapEditorPlacementKind_ != MapEditorPlacementKind::Wall &&
        mapEditorPlacementKind_ != MapEditorPlacementKind::Prop) {
        return objects;
    }

    const content::ObjectPlacementKind placementKind = mapEditorPlacementKind_ == MapEditorPlacementKind::Wall
        ? content::ObjectPlacementKind::Wall
        : content::ObjectPlacementKind::Prop;
    for (const auto& object : allObjects) {
        if (!object.editorVisible || object.placementKind != placementKind) {
            continue;
        }
        objects.push_back(&object);
    }
    return objects;
}

const content::ObjectAssetDefinition* Application::selectedMapEditorObjectAsset() const {
    const auto& objectAssets = contentDatabase_.objectAssets();
    if (objectAssets.empty() ||
        (mapEditorPlacementKind_ != MapEditorPlacementKind::Wall &&
         mapEditorPlacementKind_ != MapEditorPlacementKind::Prop)) {
        return nullptr;
    }

    const content::ObjectPlacementKind placementKind = mapEditorPlacementKind_ == MapEditorPlacementKind::Wall
        ? content::ObjectPlacementKind::Wall
        : content::ObjectPlacementKind::Prop;

    const auto isSelectable = [placementKind](const content::ObjectAssetDefinition& object) {
        return object.editorVisible && object.placementKind == placementKind;
    };

    const std::size_t clampedIndex = std::min(editorObjectAssetIndex_, objectAssets.size() - 1);
    if (isSelectable(objectAssets[clampedIndex])) {
        return &objectAssets[clampedIndex];
    }

    const auto it = std::find_if(objectAssets.begin(), objectAssets.end(), isSelectable);
    return it != objectAssets.end() ? &*it : nullptr;
}

const content::ObjectAssetDefinition* Application::selectedManagedObjectAsset() const {
    const auto& objectAssets = contentDatabase_.objectAssets();
    if (objectAssets.empty()) {
        return nullptr;
    }
    const std::size_t clampedIndex = std::min(managedObjectAssetIndex_, objectAssets.size() - 1);
    return &objectAssets[clampedIndex];
}

std::size_t Application::findObjectAssetIndexById(const std::string_view id) const {
    const auto& objectAssets = contentDatabase_.objectAssets();
    const auto it = std::find_if(objectAssets.begin(), objectAssets.end(), [id](const content::ObjectAssetDefinition& object) {
        return object.id == id;
    });
    return it != objectAssets.end()
        ? static_cast<std::size_t>(std::distance(objectAssets.begin(), it))
        : objectAssets.size();
}

std::string Application::makeNextObjectAssetId(const std::string_view seed) const {
    std::string base;
    base.reserve(seed.size() + 16);
    for (const unsigned char character : seed) {
        if (std::isalnum(character) != 0) {
            base.push_back(static_cast<char>(std::tolower(character)));
        } else if (character == '_' || character == '-' || character == ' ') {
            if (base.empty() || base.back() == '_') {
                continue;
            }
            base.push_back('_');
        }
    }
    while (!base.empty() && base.back() == '_') {
        base.pop_back();
    }
    if (base.empty()) {
        base = "custom_object";
    }

    const auto& objectAssets = contentDatabase_.objectAssets();
    auto exists = [&objectAssets](const std::string_view id) {
        return std::ranges::any_of(objectAssets, [id](const content::ObjectAssetDefinition& object) {
            return object.id == id;
        });
    };
    if (!exists(base)) {
        return base;
    }

    for (int index = 1; index <= 9999; ++index) {
        std::ostringstream candidate;
        candidate << base << '_' << std::setw(2) << std::setfill('0') << index;
        if (!exists(candidate.str())) {
            return candidate.str();
        }
    }
    return base + "_overflow";
}

int Application::countObjectAssetReferencesInMap(const gameplay::MapData& map, const std::string_view id) const {
    return static_cast<int>(std::count_if(map.props.begin(), map.props.end(), [id](const gameplay::MapProp& prop) {
        return prop.id == id;
    }));
}

int Application::countObjectAssetReferencesInStoredMaps(const std::string_view id) const {
    int count = 0;
    for (const auto& path : mapCatalogPaths_) {
        gameplay::MapData map = gameplay::MapSerializer::load(path);
        count += countObjectAssetReferencesInMap(map, id);
    }
    return count;
}

void Application::createManagedObjectAsset() {
    const content::ObjectAssetDefinition* templateAsset = selectedManagedObjectAsset();
    content::ObjectAssetDefinition definition = templateAsset != nullptr
        ? *templateAsset
        : content::ObjectAssetDefinition{};
    definition.id = makeNextObjectAssetId(templateAsset != nullptr ? templateAsset->id + "_copy" : "custom_object");
    if (definition.label.empty()) {
        definition.label = "新对象";
    } else {
        definition.label += " 副本";
    }
    if (definition.category.empty()) {
        definition.category = "自定义";
    }
    if (definition.modelPath.empty()) {
        definition.modelPath = "generated/models/crate.obj";
    }
    if (definition.previewColor.r == 0 && definition.previewColor.g == 0 && definition.previewColor.b == 0) {
        definition.previewColor = {160, 164, 170};
    }
    if (contentDatabase_.upsertObjectAsset(definition)) {
        managedObjectAssetIndex_ = findObjectAssetIndexById(definition.id);
        if (definition.editorVisible &&
            ((definition.placementKind == content::ObjectPlacementKind::Prop && mapEditorPlacementKind_ == MapEditorPlacementKind::Prop) ||
             (definition.placementKind == content::ObjectPlacementKind::Wall && mapEditorPlacementKind_ == MapEditorPlacementKind::Wall))) {
            editorObjectAssetIndex_ = managedObjectAssetIndex_;
        }
        clampManagedObjectAssetSelection();
        clampMapEditorAssetSelection();
        mapEditorStatus_ = std::string("已创建对象资产: ") + definition.label;
    } else {
        mapEditorStatus_ = "创建对象资产失败";
    }
    needsRedraw_ = true;
}

void Application::saveManagedObjectAsset(const content::ObjectAssetDefinition& definition, const std::string_view previousId) {
    const content::ObjectAssetDefinition* current = selectedManagedObjectAsset();
    if (current == nullptr) {
        mapEditorStatus_ = "当前没有可编辑的对象资产";
        needsRedraw_ = true;
        return;
    }

    if (definition.id.empty()) {
        mapEditorStatus_ = "对象资产 id 不能为空";
        needsRedraw_ = true;
        return;
    }

    const std::size_t existingIndex = findObjectAssetIndexById(definition.id);
    if (definition.id != previousId && existingIndex < contentDatabase_.objectAssets().size()) {
        mapEditorStatus_ = std::string("对象资产 id 已存在: ") + definition.id;
        needsRedraw_ = true;
        return;
    }

    if (definition.id != previousId) {
        const int activeRefs = countObjectAssetReferencesInMap(activeMap_, previousId);
        const int storedRefs = countObjectAssetReferencesInStoredMaps(previousId);
        if (activeRefs > 0 || storedRefs > 0) {
            mapEditorStatus_ = std::string("对象资产正在被地图引用，不能直接改 id: ") + std::string(previousId);
            needsRedraw_ = true;
            return;
        }
    }

    if (!contentDatabase_.upsertObjectAsset(definition)) {
        mapEditorStatus_ = std::string("保存对象资产失败: ") + definition.label;
        needsRedraw_ = true;
        return;
    }

    if (definition.id != previousId) {
        contentDatabase_.removeObjectAsset(previousId);
    }
    contentDatabase_.resolveMapData(activeMap_);
    managedObjectAssetIndex_ = findObjectAssetIndexById(definition.id);
    if (selectedMapEditorObjectAsset() != nullptr && selectedMapEditorObjectAsset()->id == previousId) {
        editorObjectAssetIndex_ = managedObjectAssetIndex_;
    }
    clampManagedObjectAssetSelection();
    clampMapEditorAssetSelection();
    mapEditorStatus_ = std::string("已保存对象资产: ") + definition.label;
    needsRedraw_ = true;
}

void Application::deleteManagedObjectAsset() {
    const content::ObjectAssetDefinition* current = selectedManagedObjectAsset();
    if (current == nullptr) {
        mapEditorStatus_ = "当前没有可删除的对象资产";
        needsRedraw_ = true;
        return;
    }

    const int activeRefs = countObjectAssetReferencesInMap(activeMap_, current->id);
    const int storedRefs = countObjectAssetReferencesInStoredMaps(current->id);
    if (activeRefs > 0 || storedRefs > 0) {
        mapEditorStatus_ = std::string("对象资产仍被地图引用，不能删除: ") + current->label;
        needsRedraw_ = true;
        return;
    }

    const std::string deletedId = current->id;
    const std::string deletedLabel = current->label;
    if (!contentDatabase_.removeObjectAsset(deletedId)) {
        mapEditorStatus_ = std::string("删除对象资产失败: ") + deletedLabel;
        needsRedraw_ = true;
        return;
    }
    clampManagedObjectAssetSelection();
    if (selectedMapEditorObjectAsset() != nullptr && selectedMapEditorObjectAsset()->id == deletedId) {
        clampMapEditorAssetSelection();
    }
    mapEditorStatus_ = std::string("已删除对象资产: ") + deletedLabel;
    needsRedraw_ = true;
}

void Application::setSelectedMapEditorPropPosition(const util::Vec3& position) {
    gameplay::MapProp* prop = selectedMapEditorProp();
    if (prop == nullptr) {
        return;
    }

    const float maxX = std::max(0.001f, static_cast<float>(std::max(activeMap_.width, 1)) - 0.001f);
    const float maxZ = std::max(0.001f, static_cast<float>(std::max(activeMap_.depth, 1)) - 0.001f);
    const float yExtent = std::max(4.0f, static_cast<float>(std::max(activeMap_.height, 1)) * 2.0f);
    const util::Vec3 clampedPosition{
        std::clamp(position.x, 0.0f, maxX),
        std::clamp(position.y, -yExtent, yExtent),
        std::clamp(position.z, 0.0f, maxZ),
    };
    if (std::abs(prop->position.x - clampedPosition.x) <= 0.0001f &&
        std::abs(prop->position.y - clampedPosition.y) <= 0.0001f &&
        std::abs(prop->position.z - clampedPosition.z) <= 0.0001f) {
        return;
    }

    pushMapEditorUndoSnapshot("调整道具位置");
    prop->position = clampedPosition;

    mapEditorTargetPosition_ = prop->position;
    mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
    mapEditorHasTarget_ = true;
    mapEditorTargetOnSurface_ = false;
    mapEditorCursorX_ = std::clamp(static_cast<int>(std::floor(prop->position.x)), 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(static_cast<int>(std::floor(prop->position.z)), 0, std::max(0, activeMap_.depth - 1));
    mapEditorStatus_ = std::string("已更新道具位置: ") + propDisplayLabel(*prop);
    syncMapEditorTargetFromView();
    needsRedraw_ = true;
}

void Application::setSelectedMapEditorPropRotation(const util::Vec3& rotationDegrees) {
    gameplay::MapProp* prop = selectedMapEditorProp();
    if (prop == nullptr) {
        return;
    }

    const util::Vec3 wrappedRotation{
        wrapDegrees(rotationDegrees.x),
        wrapDegrees(rotationDegrees.y),
        wrapDegrees(rotationDegrees.z),
    };
    if (std::abs(prop->rotationDegrees.x - wrappedRotation.x) <= 0.0001f &&
        std::abs(prop->rotationDegrees.y - wrappedRotation.y) <= 0.0001f &&
        std::abs(prop->rotationDegrees.z - wrappedRotation.z) <= 0.0001f) {
        return;
    }

    pushMapEditorUndoSnapshot("调整道具旋转");
    prop->rotationDegrees = wrappedRotation;
    mapEditorStatus_ = std::string("已更新道具旋转: ") + propDisplayLabel(*prop);
    syncMapEditorTargetFromView();
    needsRedraw_ = true;
}

void Application::setSelectedMapEditorPropScale(const util::Vec3& scale) {
    gameplay::MapProp* prop = selectedMapEditorProp();
    if (prop == nullptr) {
        return;
    }

    const util::Vec3 sanitizedScale = sanitizeEditorPropScale(scale);
    if (std::abs(prop->scale.x - sanitizedScale.x) <= 0.0001f &&
        std::abs(prop->scale.y - sanitizedScale.y) <= 0.0001f &&
        std::abs(prop->scale.z - sanitizedScale.z) <= 0.0001f) {
        return;
    }

    pushMapEditorUndoSnapshot("调整道具缩放");
    prop->scale = sanitizedScale;
    mapEditorStatus_ = std::string("已更新道具缩放: ") + propDisplayLabel(*prop);
    syncMapEditorTargetFromView();
    needsRedraw_ = true;
}

void Application::handleMapBrowserInput(const platform::InputSnapshot& input) {
    if (input.primaryClickPressed) {
        const std::size_t hitIndex = hitTestMapBrowserItem(input.mouseX, input.mouseY);
        if (hitIndex != std::numeric_limits<std::size_t>::max()) {
            loadEditorMapByIndex(hitIndex);
            activateSelectedMapBrowserItem();
        }
        return;
    }

    if (input.upPressed || input.editorPreviousMapPressed) {
        cycleEditorMap(-1);
    }
    if (input.downPressed || input.editorNextMapPressed) {
        cycleEditorMap(1);
    }
    if (input.editorNewMapPressed) {
        createNewEditorMap();
        if (mapBrowserTargetFlow_ == AppFlow::MapEditor) {
            currentFlow_ = AppFlow::MapEditor;
            syncInputMode();
            refreshWindowTitle();
            logCurrentFlow();
        }
        needsRedraw_ = true;
        return;
    }
    if (input.confirmPressed) {
        activateSelectedMapBrowserItem();
    }
}

void Application::handleMultiplayerLobbyInput(const platform::InputSnapshot& input) {
    if (input.upPressed) {
        navigateMultiplayerLobby(-1);
    }
    if (input.downPressed) {
        navigateMultiplayerLobby(1);
    }
    if (input.leftPressed) {
        adjustSelectedMultiplayerSetting(-1);
    }
    if (input.rightPressed) {
        adjustSelectedMultiplayerSetting(1);
    }
    if (input.confirmPressed) {
        activateSelectedMultiplayerSetting();
    }
}

void Application::moveMapEditorCursor(const int dx, const int dz) {
    const int nextX = std::clamp(mapEditorCursorX_ + dx, 0, std::max(0, activeMap_.width - 1));
    const int nextZ = std::clamp(mapEditorCursorZ_ + dz, 0, std::max(0, activeMap_.depth - 1));
    if (nextX == mapEditorCursorX_ && nextZ == mapEditorCursorZ_) {
        return;
    }
    mapEditorCursorX_ = nextX;
    mapEditorCursorZ_ = nextZ;
    mapEditorTargetPosition_ = centerOfCell(mapEditorCursorX_, mapEditorCursorZ_, 0.0f);
    mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
    mapEditorHasTarget_ = true;
    mapEditorTargetOnSurface_ = true;
    syncSelectedMapEditorPropFromCursor();
    if (const gameplay::MapProp* prop = selectedMapEditorProp(); prop != nullptr) {
        mapEditorStatus_ = std::string("已选中道具: ") + propDisplayLabel(*prop);
    } else {
        mapEditorStatus_ = "已移动编辑光标";
    }
    needsRedraw_ = true;
}

bool Application::selectMapEditorCellFromMouse(const int mouseX, const int mouseY) {
    if (window_ == nullptr) {
        return false;
    }

    const int width = std::max(1, window_->clientWidth());
    const int height = std::max(1, window_->clientHeight());
    constexpr float kCanvasX0 = 0.30f;
    constexpr float kCanvasY0 = 0.10f;
    constexpr float kCanvasX1 = 0.92f;
    constexpr float kCanvasY1 = 0.86f;
    const float normalizedX = static_cast<float>(mouseX) / static_cast<float>(width);
    const float normalizedY = static_cast<float>(mouseY) / static_cast<float>(height);
    if (normalizedX < kCanvasX0 || normalizedX > kCanvasX1 || normalizedY < kCanvasY0 || normalizedY > kCanvasY1) {
        return false;
    }

    const float localX = (normalizedX - kCanvasX0) / (kCanvasX1 - kCanvasX0);
    const float localY = (normalizedY - kCanvasY0) / (kCanvasY1 - kCanvasY0);
    mapEditorCursorX_ = std::clamp(static_cast<int>(localX * static_cast<float>(activeMap_.width)), 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(static_cast<int>(localY * static_cast<float>(activeMap_.depth)), 0, std::max(0, activeMap_.depth - 1));
    mapEditorTargetPosition_ = centerOfCell(mapEditorCursorX_, mapEditorCursorZ_, 0.0f);
    mapEditorTargetNormal_ = {0.0f, 1.0f, 0.0f};
    mapEditorHasTarget_ = true;
    mapEditorTargetOnSurface_ = true;
    syncSelectedMapEditorPropFromCursor();
    if (const gameplay::MapProp* prop = selectedMapEditorProp(); prop != nullptr) {
        mapEditorStatus_ = std::string("已选中道具: ") + propDisplayLabel(*prop);
    } else {
        mapEditorStatus_ = "已通过鼠标选中格子";
    }
    needsRedraw_ = true;
    return true;
}

void Application::applyMapEditorTool() {
    if (mapEditorTool_ == MapEditorTool::Select) {
        if (hoveredEditorPropIndex_.has_value()) {
            selectedEditorPropIndex_ = hoveredEditorPropIndex_;
            mapEditorStatus_ = std::string("已锁定对象: ") + propDisplayLabel(activeMap_.props[*hoveredEditorPropIndex_]);
        } else {
            mapEditorStatus_ = mapEditorViewMode_ == MapEditorViewMode::Perspective
                ? "选择工具会锁定镜头正前方的对象"
                : "点击对象即可选中";
        }
        needsRedraw_ = true;
        return;
    }

    if (mapEditorTool_ == MapEditorTool::Pan) {
        mapEditorStatus_ = "抓手工具用于浏览场景";
        needsRedraw_ = true;
        return;
    }

    if (mapEditorTool_ == MapEditorTool::Erase) {
        eraseMapEditorCell();
        return;
    }

    if (!mapEditorHasTarget_) {
        syncMapEditorTargetFromView();
    }

    switch (mapEditorPlacementKind_) {
        case MapEditorPlacementKind::Wall:
        case MapEditorPlacementKind::Prop: {
            gameplay::MapProp preview = buildMapEditorPlacementPreviewProp();
            pushMapEditorUndoSnapshot("放置对象");
            activeMap_.props.push_back(preview);
            selectedEditorPropIndex_ = activeMap_.props.size() - 1;
            mapEditorStatus_ = std::string("已放置对象: ")
                + propDisplayLabel(preview)
                + " @ "
                + std::to_string(static_cast<int>(std::lround(preview.position.x * 10.0f)) / 10.0f)
                + ", "
                + std::to_string(static_cast<int>(std::lround(preview.position.y * 10.0f)) / 10.0f)
                + ", "
                + std::to_string(static_cast<int>(std::lround(preview.position.z * 10.0f)) / 10.0f);
            break;
        }
        case MapEditorPlacementKind::AttackerSpawn:
        case MapEditorPlacementKind::DefenderSpawn: {
            pushMapEditorUndoSnapshot("放置出生点");
            const gameplay::SpawnPoint spawn = buildMapEditorPlacementPreviewSpawn();
            activeMap_.spawns.push_back(spawn);
            selectedEditorPropIndex_.reset();
            mapEditorStatus_ = spawn.team == gameplay::Team::Attackers
                ? "已新增进攻出生点"
                : "已新增防守出生点";
            break;
        }
    }

    syncMapEditorTargetFromView();
    needsRedraw_ = true;
}

void Application::eraseMapEditorCell() {
    if (hoveredEditorPropIndex_.has_value() && *hoveredEditorPropIndex_ < activeMap_.props.size()) {
        pushMapEditorUndoSnapshot("删除对象");
        const std::string label = propDisplayLabel(activeMap_.props[*hoveredEditorPropIndex_]);
        activeMap_.props.erase(activeMap_.props.begin() + static_cast<std::ptrdiff_t>(*hoveredEditorPropIndex_));
        hoveredEditorPropIndex_.reset();
        selectedEditorPropIndex_.reset();
        syncMapEditorTargetFromView();
        mapEditorStatus_ = std::string("已删除对象: ") + label;
        needsRedraw_ = true;
        return;
    }

    if (hoveredEditorSpawnIndex_.has_value() && *hoveredEditorSpawnIndex_ < activeMap_.spawns.size()) {
        pushMapEditorUndoSnapshot("删除出生点");
        const gameplay::Team team = activeMap_.spawns[*hoveredEditorSpawnIndex_].team;
        activeMap_.spawns.erase(activeMap_.spawns.begin() + static_cast<std::ptrdiff_t>(*hoveredEditorSpawnIndex_));
        hoveredEditorSpawnIndex_.reset();
        mapEditorStatus_ = team == gameplay::Team::Attackers ? "已删除进攻出生点" : "已删除防守出生点";
        syncMapEditorTargetFromView();
        needsRedraw_ = true;
        return;
    }

    if (selectedEditorPropIndex_.has_value() && *selectedEditorPropIndex_ < activeMap_.props.size()) {
        pushMapEditorUndoSnapshot("删除已选对象");
        const std::string label = propDisplayLabel(activeMap_.props[*selectedEditorPropIndex_]);
        activeMap_.props.erase(activeMap_.props.begin() + static_cast<std::ptrdiff_t>(*selectedEditorPropIndex_));
        selectedEditorPropIndex_.reset();
        syncMapEditorTargetFromView();
        mapEditorStatus_ = std::string("已删除对象: ") + label;
        needsRedraw_ = true;
        return;
    }

    syncMapEditorTargetFromView();
    mapEditorStatus_ = "当前目标没有可删除对象";
    needsRedraw_ = true;
}

void Application::openMapBrowser(const AppFlow targetFlow) {
    mapBrowserTargetFlow_ = targetFlow;
    refreshMapCatalog();
    if (!mapCatalogPaths_.empty()) {
        loadEditorMapByIndex(activeMapCatalogIndex_);
    } else {
        mapEditorStatus_ = "没有可用地图";
    }
    currentFlow_ = AppFlow::MapBrowser;
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::activateSelectedMapBrowserItem() {
    refreshMapCatalog();
    if (!mapCatalogPaths_.empty()) {
        loadEditorMapByIndex(activeMapCatalogIndex_);
    }

    currentFlow_ = mapBrowserTargetFlow_;
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        initializeSinglePlayerView();
        restartNetworkSession(network::SessionType::Offline, "切换到单机离线模式");
    } else if (currentFlow_ == AppFlow::MapEditor) {
        initializeMapEditorView();
        mapEditorStatus_ = std::string("正在编辑地图: ") + activeMapPath_.stem().string();
    } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
        selectedMultiplayerIndex_ = 0;
        multiplayerStatus_ = std::string("已选择地图: ") + activeMapPath_.stem().string();
    }
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::navigateMultiplayerLobby(const int delta) {
    const int count = static_cast<int>(kMultiplayerEntryCount);
    const int current = static_cast<int>(selectedMultiplayerIndex_);
    selectedMultiplayerIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    needsRedraw_ = true;
}

void Application::adjustSelectedMultiplayerSetting(const int delta) {
    switch (selectedMultiplayerIndex_) {
        case 0:
            multiplayerSessionType_ = multiplayerSessionType_ == network::SessionType::Host
                ? network::SessionType::Client
                : network::SessionType::Host;
            multiplayerStatus_ = std::string("联机模式切换为") + sessionTypeLabel(multiplayerSessionType_);
            needsRedraw_ = true;
            break;
        case 1: {
            const int nextPort = std::clamp(static_cast<int>(settings_.network.port) + delta * 5, 1, 65535);
            settings_.network.port = static_cast<std::uint16_t>(nextPort);
            saveSettings(settings_, settingsPath_);
            multiplayerStatus_ = "端口已更新";
            needsRedraw_ = true;
            break;
        }
        case 2: {
            settings_.network.maxPlayers = std::clamp(settings_.network.maxPlayers + delta, 2, 32);
            saveSettings(settings_, settingsPath_);
            multiplayerStatus_ = "房间人数已更新";
            needsRedraw_ = true;
            break;
        }
        case 3:
            if (delta != 0) {
                activateSelectedMultiplayerSetting();
            }
            break;
        default:
            break;
    }
}

void Application::activateSelectedMultiplayerSetting() {
    if (selectedMultiplayerIndex_ == 3) {
        restartNetworkSession(multiplayerSessionType_, "联机房间已启动");
        if (!multiplayerSessionActive_) {
            multiplayerStatus_ = std::string("启动失败: ") + settings_.network.defaultServerHost + ":" +
                std::to_string(settings_.network.port);
            needsRedraw_ = true;
            return;
        }

        receivedMultiplayerSnapshot_ = false;
        appliedNetworkMapRevision_ = 0;
        if (isRemoteClientSession()) {
            multiplayerGameplayReady_ = false;
            physicsWorld_.shutdown();
            activeMap_ = gameplay::MapData{
                .name = "等待主机地图",
                .width = 1,
                .height = 2,
                .depth = 1,
                .spawns = {},
                .props = {},
                .lights = {},
            };
            simulation_ = gameplay::SimulationWorld(activeMap_);
            singlePlayerCameraPosition_ = {0.5f, kSinglePlayerEyeHeight, 0.5f};
            singlePlayerCameraYawRadians_ = 0.0f;
            singlePlayerCameraPitchRadians_ = 0.0f;
            multiplayerStatus_ = "ENet 客户端已进入训练场，等待主机地图...";
        } else {
            initializeSinglePlayerView();
            multiplayerStatus_ = std::string("ENet 主机已启动，地图 ") + activeMap_.name + "，正在广播地图与训练场快照";
        }
        currentFlow_ = AppFlow::SinglePlayerLobby;
        syncInputMode();
        refreshWindowTitle();
        logCurrentFlow();
        needsRedraw_ = true;
        return;
    }
    adjustSelectedMultiplayerSetting(1);
}

void Application::restartNetworkSession(const network::SessionType type, const char* reason) {
    networkSession_.stop();
    receivedMultiplayerSnapshot_ = false;
    networkSession_ = network::NetworkSession({
        .type = type,
        .endpoint = {settings_.network.defaultServerHost, settings_.network.port},
        .maxPeers = static_cast<std::size_t>(std::max(2, settings_.network.maxPlayers)),
        .localPlayerId = makeSessionLocalPlayerId(type, settings_.network.playerName),
        .localPlayerDisplayName = settings_.network.playerName,
    });
    const bool started = networkSession_.start();
    multiplayerSessionActive_ = type != network::SessionType::Offline && started;
    if (!started) {
        spdlog::error("[Network] {} 失败: {}:{}",
            reason,
            settings_.network.defaultServerHost,
            settings_.network.port);
        return;
    }
    spdlog::info("[Network] {} | type={} endpoint={}:{} maxPlayers={}",
        reason,
        sessionTypeLabel(type),
        settings_.network.defaultServerHost,
        settings_.network.port,
        settings_.network.maxPlayers);
}

void Application::refreshMapCatalog() {
    mapCatalogPaths_.clear();
    const auto mapsRoot = assetRoot_ / "maps";
    std::error_code error;
    if (std::filesystem::exists(mapsRoot, error) && !error) {
        for (const auto& entry : std::filesystem::directory_iterator(mapsRoot, error)) {
            if (error || !entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() == ".arena") {
                mapCatalogPaths_.push_back(entry.path());
            }
        }
    }

    std::sort(mapCatalogPaths_.begin(), mapCatalogPaths_.end());
    if (mapCatalogPaths_.empty()) {
        activeMapPath_ = mapsRoot / "depot_lab.arena";
        activeMapCatalogIndex_ = 0;
        return;
    }

    auto match = std::find(mapCatalogPaths_.begin(), mapCatalogPaths_.end(), activeMapPath_);
    if (match == mapCatalogPaths_.end()) {
        activeMapPath_ = mapCatalogPaths_.front();
        activeMapCatalogIndex_ = 0;
    } else {
        activeMapCatalogIndex_ = static_cast<std::size_t>(std::distance(mapCatalogPaths_.begin(), match));
    }
}

void Application::loadEditorMapByIndex(const std::size_t index) {
    refreshMapCatalog();
    if (mapCatalogPaths_.empty()) {
        return;
    }

    activeMapCatalogIndex_ = std::min(index, mapCatalogPaths_.size() - 1);
    activeMapPath_ = mapCatalogPaths_[activeMapCatalogIndex_];
    gameplay::MapData loadedMap = gameplay::MapSerializer::load(activeMapPath_);
    contentDatabase_.resolveMapData(loadedMap);
    if (!loadedMap.props.empty() || !loadedMap.spawns.empty() || !loadedMap.name.empty()) {
        activeMap_ = loadedMap;
    }
    clearMapEditorUndoHistory();
    mapEditorCursorX_ = std::clamp(mapEditorCursorX_, 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(mapEditorCursorZ_, 0, std::max(0, activeMap_.depth - 1));
    initializeMapEditorView();
    mapEditorStatus_ = std::string("已加载地图: ") + activeMapPath_.stem().string();
    needsRedraw_ = true;
    spdlog::info("[MapEditor] Loaded map: {}", activeMapPath_.generic_string());
}

void Application::cycleEditorMap(const int delta) {
    refreshMapCatalog();
    if (mapCatalogPaths_.empty()) {
        return;
    }

    const int count = static_cast<int>(mapCatalogPaths_.size());
    const int current = static_cast<int>(activeMapCatalogIndex_);
    const std::size_t nextIndex = static_cast<std::size_t>((current + delta + count) % count);
    loadEditorMapByIndex(nextIndex);
}

gameplay::MapData Application::makeBlankEditorMap(const std::string& name) const {
    gameplay::MapData map{
        .name = name,
        .width = 24,
        .height = 8,
        .depth = 24,
        .spawns = {},
        .props = {},
        .lights = {},
    };

    constexpr float kFloorThickness = 0.10f;
    constexpr float kWallThickness = 0.50f;
    constexpr float kWallHeight = 3.20f;
    const float width = static_cast<float>(map.width);
    const float depth = static_cast<float>(map.depth);

    map.props.push_back(contentDatabase_.instantiateMapProp(
        "editor_brush_floor",
        {width * 0.5f, -kFloorThickness, depth * 0.5f},
        {},
        {width, kFloorThickness, depth}));
    map.props.push_back(contentDatabase_.instantiateMapProp(
        "editor_brush_wall",
        {kWallThickness * 0.5f, 0.0f, depth * 0.5f},
        {},
        {kWallThickness, kWallHeight, depth}));
    map.props.push_back(contentDatabase_.instantiateMapProp(
        "editor_brush_wall",
        {width - kWallThickness * 0.5f, 0.0f, depth * 0.5f},
        {},
        {kWallThickness, kWallHeight, depth}));
    map.props.push_back(contentDatabase_.instantiateMapProp(
        "editor_brush_wall",
        {width * 0.5f, 0.0f, kWallThickness * 0.5f},
        {},
        {width, kWallHeight, kWallThickness}));
    map.props.push_back(contentDatabase_.instantiateMapProp(
        "editor_brush_wall",
        {width * 0.5f, 0.0f, depth - kWallThickness * 0.5f},
        {},
        {width, kWallHeight, kWallThickness}));

    map.spawns.push_back(gameplay::SpawnPoint{gameplay::Team::Attackers, {3.5f, 1.0f, 3.5f}});
    map.spawns.push_back(gameplay::SpawnPoint{gameplay::Team::Defenders, {20.5f, 1.0f, 20.5f}});
    map.lights.push_back(gameplay::LightProbe{{12.0f, 6.0f, 12.0f}, {1.0f, 0.96f, 0.86f}, 9.0f});
    return map;
}

std::filesystem::path Application::nextCustomMapPath() const {
    const auto mapsRoot = assetRoot_ / "maps";
    for (int index = 1; index <= 999; ++index) {
        std::ostringstream name;
        name << "custom_map_" << std::setw(2) << std::setfill('0') << index << ".arena";
        const auto candidate = mapsRoot / name.str();
        if (!util::FileSystem::exists(candidate)) {
            return candidate;
        }
    }
    return mapsRoot / "custom_map_overflow.arena";
}

void Application::createNewEditorMap() {
    activeMapPath_ = nextCustomMapPath();
    activeMap_ = makeBlankEditorMap("Custom Arena " + activeMapPath_.stem().string());
    clearMapEditorUndoHistory();
    mapEditorCursorX_ = 3;
    mapEditorCursorZ_ = 3;
    initializeMapEditorView();
    saveActiveMapArtifacts("新建地图");
    refreshMapCatalog();
    mapEditorStatus_ = std::string("已新建地图: ") + activeMapPath_.stem().string();
    needsRedraw_ = true;
    spdlog::info("[MapEditor] Created new map slot: {}", activeMapPath_.generic_string());
}

void Application::saveActiveMapArtifacts(const char* reason) {
    if (activeMapPath_.empty()) {
        activeMapPath_ = assetRoot_ / "maps" / "depot_lab.arena";
    }
    contentDatabase_.resolveMapData(activeMap_);
    gameplay::MapSerializer::save(activeMap_, activeMapPath_);
    const auto previewPath = assetRoot_ / "generated" / (activeMapPath_.stem().string() + "_preview.ppm");
    gameplay::MapEditor(activeMap_).exportTopDownPreview(previewPath);
    refreshMapCatalog();
    spdlog::info("[MapEditor] {} -> {}", reason, activeMapPath_.generic_string());
}

const char* Application::mapEditorToolLabel() const {
    switch (mapEditorTool_) {
        case MapEditorTool::Select:
            return "选择";
        case MapEditorTool::Pan:
            return "抓手";
        case MapEditorTool::Place:
            return "放置";
        case MapEditorTool::Erase:
            return "擦除";
    }
    return "选择";
}

const char* Application::mapEditorPlacementKindLabel() const {
    switch (mapEditorPlacementKind_) {
        case MapEditorPlacementKind::Wall:
            return "盒体墙";
        case MapEditorPlacementKind::Prop:
            return "道具";
        case MapEditorPlacementKind::AttackerSpawn:
            return "进攻出生点";
        case MapEditorPlacementKind::DefenderSpawn:
            return "防守出生点";
    }
    return "道具";
}

const char* Application::mapEditorViewModeLabel() const {
    switch (mapEditorViewMode_) {
        case MapEditorViewMode::Perspective:
            return "自由镜头";
        case MapEditorViewMode::Ortho25D:
            return "2.5D 正交";
    }
    return "自由镜头";
}

void Application::persistSettings(const char* reason) {
    saveSettings(settings_, settingsPath_);
    needsRedraw_ = true;
    spdlog::info("[Settings] {} | 灵敏度={} 垂直倍率={} 俯仰上限={} 自动换弹={}",
        reason,
        settings_.gameplay.mouseSensitivity,
        settings_.gameplay.mouseVerticalSensitivity,
        settings_.gameplay.maxLookPitchDegrees,
        settingToggleLabel(settings_.gameplay.autoReload));
}

bool Application::lineOfSightBlocked(const util::Vec3& from, const util::Vec3& to) const {
    if (physicsWorld_.isReady()) {
        float hitFraction = 1.0f;
        if (!physicsWorld_.castRay(from, to, &hitFraction)) {
            return false;
        }
        return hitFraction < 0.999f;
    }

    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.001f) {
        return false;
    }

    const int steps = std::max(1, static_cast<int>(distance * 12.0f));
    for (int step = 1; step < steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const float sampleX = from.x + dx * t;
        const float sampleZ = from.z + dz * t;
        if (collidesWithWorld({sampleX, 1.0f, sampleZ})) {
            return true;
        }
    }
    return false;
}

std::size_t Application::hitTestMainMenuItem(const int mouseX, const int mouseY) const {
    const int width = window_ != nullptr ? window_->clientWidth() : 0;
    const int height = window_ != nullptr ? window_->clientHeight() : 0;
    if (width <= 0 || height <= 0) {
        return std::numeric_limits<std::size_t>::max();
    }

    const auto inRect = [mouseX, mouseY, width, height](float x0, float y0, float x1, float y1) {
        const int left = static_cast<int>(width * x0);
        const int top = static_cast<int>(height * y0);
        const int right = static_cast<int>(width * x1);
        const int bottom = static_cast<int>(height * y1);
        return mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom;
    };

    const auto& items = mainMenu_.items();
    for (std::size_t index = 0; index < items.size(); ++index) {
        const float top = 0.24f + static_cast<float>(index) * 0.11f;
        const float bottom = top + 0.085f;
        if (inRect(0.045f, top, 0.44f, bottom)) {
            return index;
        }
    }

    return std::numeric_limits<std::size_t>::max();
}

std::size_t Application::hitTestMapBrowserItem(const int mouseX, const int mouseY) const {
    const int width = window_ != nullptr ? window_->clientWidth() : 0;
    const int height = window_ != nullptr ? window_->clientHeight() : 0;
    if (width <= 0 || height <= 0 || mapCatalogPaths_.empty()) {
        return std::numeric_limits<std::size_t>::max();
    }

    const auto inRect = [mouseX, mouseY, width, height](float x0, float y0, float x1, float y1) {
        const int left = static_cast<int>(width * x0);
        const int top = static_cast<int>(height * y0);
        const int right = static_cast<int>(width * x1);
        const int bottom = static_cast<int>(height * y1);
        return mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom;
    };

    for (std::size_t index = 0; index < mapCatalogPaths_.size(); ++index) {
        const float top = 0.24f + static_cast<float>(index) * 0.10f;
        const float bottom = top + 0.075f;
        if (inRect(0.08f, top, 0.62f, bottom)) {
            return index;
        }
    }

    return std::numeric_limits<std::size_t>::max();
}

}  // namespace mycsg::app
