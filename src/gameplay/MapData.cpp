#include "gameplay/MapData.h"

#include "util/FileSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace mycsg::gameplay {

namespace {

std::string rampMaterialId(const RampDirection direction, const std::string& materialId) {
    return std::string(rampDirectionLabel(direction)) + "_" + materialId;
}

std::string teamToString(const Team team) {
    switch (team) {
        case Team::Attackers: return "attackers";
        case Team::Defenders: return "defenders";
        case Team::Neutral: return "neutral";
    }
    return "neutral";
}

Team parseTeam(const std::string& value) {
    if (value == "attackers") return Team::Attackers;
    if (value == "defenders") return Team::Defenders;
    return Team::Neutral;
}

std::string serialize(const MapData& map) {
    std::ostringstream out;
    out << "name " << map.name << '\n';
    out << "size " << map.width << ' ' << map.height << ' ' << map.depth << '\n';
    for (const auto& block : map.blocks) {
        out << "block " << block.cell.x << ' ' << block.cell.y << ' ' << block.cell.z
            << ' ' << block.materialId << ' ' << (block.solid ? 1 : 0) << '\n';
    }
    for (const auto& spawn : map.spawns) {
        out << "spawn " << teamToString(spawn.team) << ' '
            << spawn.position.x << ' ' << spawn.position.y << ' ' << spawn.position.z << '\n';
    }
    for (const auto& prop : map.props) {
        out << "prop " << prop.id << ' '
            << prop.position.x << ' ' << prop.position.y << ' ' << prop.position.z << ' '
            << prop.modelPath.generic_string() << ' ' << prop.materialPath.generic_string() << '\n';
    }
    for (const auto& light : map.lights) {
        out << "light " << light.position.x << ' ' << light.position.y << ' ' << light.position.z << ' '
            << light.color.x << ' ' << light.color.y << ' ' << light.color.z << ' '
            << light.intensity << '\n';
    }
    return out.str();
}

}  // namespace

bool MapSerializer::save(const MapData& map, const std::filesystem::path& path) {
    return util::FileSystem::writeText(path, serialize(map));
}

MapData MapSerializer::load(const std::filesystem::path& path) {
    MapData map;
    const std::string content = util::FileSystem::readText(path);
    if (content.empty()) {
        return map;
    }

    std::istringstream stream(content);
    std::string token;
    while (stream >> token) {
        if (token == "name") {
            std::getline(stream >> std::ws, map.name);
        } else if (token == "size") {
            stream >> map.width >> map.height >> map.depth;
        } else if (token == "block") {
            MapBlock block;
            int solid = 1;
            stream >> block.cell.x >> block.cell.y >> block.cell.z >> block.materialId >> solid;
            block.solid = solid != 0;
            map.blocks.push_back(block);
        } else if (token == "spawn") {
            SpawnPoint spawn;
            std::string teamValue;
            stream >> teamValue >> spawn.position.x >> spawn.position.y >> spawn.position.z;
            spawn.team = parseTeam(teamValue);
            map.spawns.push_back(spawn);
        } else if (token == "prop") {
            MapProp prop;
            std::string model;
            std::string material;
            stream >> prop.id >> prop.position.x >> prop.position.y >> prop.position.z >> model >> material;
            prop.modelPath = model;
            prop.materialPath = material;
            map.props.push_back(prop);
        } else if (token == "light") {
            LightProbe light;
            stream >> light.position.x >> light.position.y >> light.position.z
                   >> light.color.x >> light.color.y >> light.color.z
                   >> light.intensity;
            map.lights.push_back(light);
        }
    }

    return map;
}

MapEditor::MapEditor(MapData map) : map_(std::move(map)) {}

void MapEditor::paintFloor(const int y, const std::string& materialId) {
    for (int z = 0; z < map_.depth; ++z) {
        for (int x = 0; x < map_.width; ++x) {
            map_.blocks.push_back(MapBlock{{x, y, z}, materialId, true});
        }
    }
}

void MapEditor::paintPerimeterWalls(const int wallHeight, const std::string& materialId) {
    for (int y = 1; y <= wallHeight; ++y) {
        for (int z = 0; z < map_.depth; ++z) {
            map_.blocks.push_back(MapBlock{{0, y, z}, materialId, true});
            map_.blocks.push_back(MapBlock{{map_.width - 1, y, z}, materialId, true});
        }
        for (int x = 1; x < map_.width - 1; ++x) {
            map_.blocks.push_back(MapBlock{{x, y, 0}, materialId, true});
            map_.blocks.push_back(MapBlock{{x, y, map_.depth - 1}, materialId, true});
        }
    }
}

void MapEditor::addCrate(const util::Vec3& position, const std::filesystem::path& modelPath, const std::filesystem::path& materialPath) {
    map_.props.push_back(MapProp{"crate", position, modelPath, materialPath});
}

void MapEditor::addRamp(const Int3& cell, const RampDirection direction, const std::string& materialId) {
    map_.blocks.push_back(MapBlock{cell, rampMaterialId(direction, materialId), false});
}

void MapEditor::addSpawn(const Team team, const util::Vec3& position) {
    map_.spawns.push_back(SpawnPoint{team, position});
}

bool MapEditor::exportTopDownPreview(const std::filesystem::path& path) const {
    std::ostringstream out;
    out << "P3\n" << map_.width << ' ' << map_.depth << "\n255\n";

    for (int z = 0; z < map_.depth; ++z) {
        for (int x = 0; x < map_.width; ++x) {
            int highestY = -1;
            std::string material = "void";
            for (const auto& block : map_.blocks) {
                if (block.cell.x == x && block.cell.z == z && block.cell.y >= highestY) {
                    highestY = block.cell.y;
                    material = block.materialId;
                }
            }

            util::ColorRgb8 color{28, 30, 36};
            if (material.find("concrete") != std::string::npos) color = {112, 114, 118};
            if (material.find("floor") != std::string::npos) color = {92, 100, 84};
            if (material.find("site_a") != std::string::npos) color = {174, 72, 56};
            if (material.find("site_b") != std::string::npos) color = {62, 96, 172};
            if (isRampMaterial(material)) color = {188, 164, 92};

            bool hasProp = false;
            bool hasBarrel = false;
            for (const auto& prop : map_.props) {
                if (static_cast<int>(std::round(prop.position.x)) == x && static_cast<int>(std::round(prop.position.z)) == z) {
                    hasProp = true;
                    if (prop.id.find("barrel") != std::string::npos) {
                        hasBarrel = true;
                    }
                }
            }
            const bool hasSpawn = std::ranges::any_of(map_.spawns, [x, z](const auto& spawn) {
                return static_cast<int>(std::round(spawn.position.x)) == x && static_cast<int>(std::round(spawn.position.z)) == z;
            });

            if (hasProp) color = hasBarrel ? util::ColorRgb8{148, 82, 64} : util::ColorRgb8{165, 124, 68};
            if (hasSpawn) color = {68, 180, 105};

            out << static_cast<int>(color.r) << ' ' << static_cast<int>(color.g) << ' ' << static_cast<int>(color.b) << '\n';
        }
    }

    return util::FileSystem::writeText(path, out.str());
}

MapData makeDefaultBombDefusalMap(const std::filesystem::path& assetRoot) {
    const auto crateModel = assetRoot / "source" / "polyhaven" / "models" / "wooden_crate_02" / "wooden_crate_02_1k.gltf";
    const auto crateMaterial = assetRoot / "generated" / "materials" / "polyhaven_wooden_crate_02.mat";
    const auto barrelModel = assetRoot / "source" / "polyhaven" / "models" / "Barrel_02" / "Barrel_02_1k.gltf";
    const auto barrelMaterial = assetRoot / "generated" / "materials" / "polyhaven_barrel_02.mat";
    MapEditor editor(MapData{
        .name = "Depot Lab",
        .width = 24,
        .height = 8,
        .depth = 24,
        .blocks = {},
        .spawns = {},
        .props = {},
        .lights = {},
    });
    editor.paintFloor(0, "floor_concrete");
    editor.paintPerimeterWalls(3, "wall_concrete");
    editor.addSpawn(Team::Attackers, {3.0f, 1.0f, 3.0f});
    editor.addSpawn(Team::Defenders, {20.0f, 1.0f, 20.0f});
    editor.addCrate({7.0f, 0.0f, 7.0f}, crateModel, crateMaterial);
    editor.addCrate({8.0f, 0.0f, 7.0f}, crateModel, crateMaterial);
    editor.addCrate({16.0f, 0.0f, 14.0f}, crateModel, crateMaterial);

    auto& map = editor.map();
    map.props.push_back(MapProp{"barrel_02", {10.5f, 0.0f, 8.5f}, barrelModel, barrelMaterial});
    map.props.push_back(MapProp{"barrel_02", {15.5f, 0.0f, 15.5f}, barrelModel, barrelMaterial});
    map.props.push_back(MapProp{"crate_stack", {11.5f, 0.0f, 16.5f}, crateModel, crateMaterial});
    map.blocks.push_back(MapBlock{{11, 0, 11}, "bomb_site_a", true});
    map.blocks.push_back(MapBlock{{12, 0, 11}, "bomb_site_a", true});
    map.blocks.push_back(MapBlock{{11, 0, 12}, "bomb_site_a", true});
    map.blocks.push_back(MapBlock{{18, 0, 6}, "bomb_site_b", true});
    map.blocks.push_back(MapBlock{{19, 0, 6}, "bomb_site_b", true});
    map.blocks.push_back(MapBlock{{18, 0, 7}, "bomb_site_b", true});
    map.lights.push_back(LightProbe{{12.0f, 6.0f, 12.0f}, {1.0f, 0.96f, 0.86f}, 9.0f});
    map.lights.push_back(LightProbe{{6.0f, 5.0f, 18.0f}, {0.8f, 0.9f, 1.0f}, 5.5f});
    return map;
}

bool isRampMaterial(const std::string& materialId) {
    return materialId.rfind("ramp_", 0) == 0;
}

const char* rampDirectionLabel(const RampDirection direction) {
    switch (direction) {
        case RampDirection::North: return "ramp_north";
        case RampDirection::South: return "ramp_south";
        case RampDirection::East: return "ramp_east";
        case RampDirection::West: return "ramp_west";
    }
    return "ramp_north";
}

float sampleFloorHeight(const MapData& map, const float x, const float z) {
    if (x < 0.0f || z < 0.0f || x >= static_cast<float>(map.width) || z >= static_cast<float>(map.depth)) {
        return 0.0f;
    }

    const int cellX = static_cast<int>(std::floor(x));
    const int cellZ = static_cast<int>(std::floor(z));
    const float localX = x - static_cast<float>(cellX);
    const float localZ = z - static_cast<float>(cellZ);

    float floorHeight = 0.0f;
    for (const auto& block : map.blocks) {
        if (block.cell.x != cellX || block.cell.z != cellZ) {
            continue;
        }

        floorHeight = std::max(floorHeight, static_cast<float>(block.cell.y));
        if (!isRampMaterial(block.materialId)) {
            continue;
        }

        float rampHeight = 0.0f;
        if (block.materialId.rfind("ramp_north", 0) == 0) rampHeight = 1.0f - localZ;
        else if (block.materialId.rfind("ramp_south", 0) == 0) rampHeight = localZ;
        else if (block.materialId.rfind("ramp_east", 0) == 0) rampHeight = localX;
        else if (block.materialId.rfind("ramp_west", 0) == 0) rampHeight = 1.0f - localX;

        floorHeight = std::max(floorHeight, static_cast<float>(block.cell.y) + std::clamp(rampHeight, 0.0f, 1.0f));
    }

    return floorHeight;
}

}  // namespace mycsg::gameplay
