#pragma once

#include "util/MathTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mycsg::gameplay {

enum class Team {
    Neutral,
    Attackers,
    Defenders
};

enum class RampDirection {
    North,
    South,
    East,
    West
};

struct Int3 {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct MapBlock {
    Int3 cell;
    std::string materialId;
    bool solid = true;
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
    std::vector<MapBlock> blocks;
    std::vector<SpawnPoint> spawns;
    std::vector<MapProp> props;
    std::vector<LightProbe> lights;
};

class MapSerializer {
public:
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
    void addCrate(const util::Vec3& position, const std::filesystem::path& modelPath, const std::filesystem::path& materialPath);
    void addRamp(const Int3& cell, RampDirection direction, const std::string& materialId);
    void addSpawn(Team team, const util::Vec3& position);
    bool exportTopDownPreview(const std::filesystem::path& path) const;

private:
    MapData map_;
};

MapData makeDefaultBombDefusalMap(const std::filesystem::path& assetRoot);

bool isRampMaterial(const std::string& materialId);
float sampleFloorHeight(const MapData& map, float x, float z);
const char* rampDirectionLabel(RampDirection direction);

}  // namespace mycsg::gameplay
