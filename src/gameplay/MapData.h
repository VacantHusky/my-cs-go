#pragma once

#include "util/MathTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mycsg::gameplay {

enum class Team {
    Neutral,
    Attackers,
    Defenders
};

struct SpawnPoint {
    Team team = Team::Neutral;
    util::Vec3 position;
};

struct MapProp {
    std::string id;
    util::Vec3 position;
    std::filesystem::path modelPath;
    std::filesystem::path materialPath;
    std::string label;
    std::string category;
    util::Vec3 collisionHalfExtents{0.42f, 0.52f, 0.42f};
    util::Vec3 collisionCenterOffset{0.0f, 0.52f, 0.0f};
    util::ColorRgb8 previewColor{160, 164, 170};
    bool cylindricalFootprint = false;
    util::Vec3 rotationDegrees{};
    util::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct LightProbe {
    util::Vec3 position;
    util::Vec3 color;
    float intensity = 1.0f;
};

struct MapData {
    std::string name = "Training Ground";
    int width = 24;
    int height = 8;
    int depth = 24;
    std::vector<SpawnPoint> spawns;
    std::vector<MapProp> props;
    std::vector<LightProbe> lights;
};

class MapSerializer {
public:
    static std::string serialize(const MapData& map);
    static MapData deserialize(std::string_view content);
    static bool save(const MapData& map, const std::filesystem::path& path);
    static MapData load(const std::filesystem::path& path);
};

class MapEditor {
public:
    explicit MapEditor(MapData map = {});

    MapData& map() { return map_; }
    const MapData& map() const { return map_; }

    void paintFloor(int y, const std::string& materialId);
    void paintPerimeterWalls(int wallHeight, const std::string& materialId);
    void addObject(const std::string& objectId,
                   const util::Vec3& position,
                   const std::filesystem::path& modelPath,
                   const std::filesystem::path& materialPath);
    void addSpawn(Team team, const util::Vec3& position);
    bool exportTopDownPreview(const std::filesystem::path& path) const;

private:
    MapData map_;
};

MapData makeDefaultBombDefusalMap(const std::filesystem::path& assetRoot);
MapData makeMetroStationShowcaseMap(const std::filesystem::path& assetRoot);

float sampleFloorHeight(const MapData& map, float x, float z);

}  // namespace mycsg::gameplay
