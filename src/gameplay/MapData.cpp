#include "gameplay/MapData.h"

#include "util/FileSystem.h"

#include <simdjson.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace mycsg::gameplay {

namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
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

std::string escapeJsonString(const std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    std::ostringstream code;
                    code << "\\u"
                         << std::hex << std::setw(4) << std::setfill('0')
                         << static_cast<int>(static_cast<unsigned char>(character));
                    escaped += code.str();
                } else {
                    escaped.push_back(character);
                }
                break;
        }
    }
    return escaped;
}

void writeVec3Array(std::ostringstream& out, const util::Vec3& value) {
    out << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

util::Vec3 previewPropHalfExtents(const MapProp& prop) {
    const std::string key = lowerAscii(prop.id + " " + prop.modelPath.generic_string());
    const util::Vec3 scale{
        std::max(0.05f, std::abs(prop.scale.x)),
        std::max(0.05f, std::abs(prop.scale.y)),
        std::max(0.05f, std::abs(prop.scale.z)),
    };

    if (key.find("editor_brush") != std::string::npos ||
        lowerAscii(prop.modelPath.filename().string()) == "crate.obj") {
        return {0.5f * scale.x, 0.5f * scale.y, 0.5f * scale.z};
    }
    if (key.find("barrel") != std::string::npos) {
        return {0.34f * scale.x, 0.55f * scale.y, 0.34f * scale.z};
    }
    if (key.find("crate") != std::string::npos) {
        return {0.48f * scale.x, 0.48f * scale.y, 0.48f * scale.z};
    }
    return {0.42f * scale.x, 0.52f * scale.y, 0.42f * scale.z};
}

bool previewPropHitsFootprint(const MapProp& prop, const float x, const float z) {
    const std::string key = lowerAscii(prop.id + " " + prop.modelPath.generic_string());
    const util::Vec3 half = previewPropHalfExtents(prop);
    const float dx = x - prop.position.x;
    const float dz = z - prop.position.z;
    const float yawRadians = prop.rotationDegrees.y * (3.1415926535f / 180.0f);
    const float cosine = std::cos(yawRadians);
    const float sine = std::sin(yawRadians);
    const float localX = dx * cosine + dz * sine;
    const float localZ = -dx * sine + dz * cosine;

    if (key.find("barrel") != std::string::npos) {
        const float radius = std::max(half.x, half.z);
        return localX * localX + localZ * localZ <= radius * radius;
    }
    return std::abs(localX) <= half.x && std::abs(localZ) <= half.z;
}

util::ColorRgb8 previewPropColor(const MapProp& prop) {
    const std::string key = lowerAscii(prop.id + " " + prop.modelPath.generic_string() + " " + prop.materialPath.generic_string());
    if (key.find("bomb_site_a") != std::string::npos || key.find("site_a") != std::string::npos) {
        return {174, 72, 56};
    }
    if (key.find("bomb_site_b") != std::string::npos || key.find("site_b") != std::string::npos) {
        return {62, 96, 172};
    }
    if (key.find("floor") != std::string::npos) {
        return {92, 100, 84};
    }
    if (key.find("wall") != std::string::npos || key.find("concrete") != std::string::npos) {
        return {112, 114, 118};
    }
    if (key.find("barrel") != std::string::npos) {
        return {148, 82, 64};
    }
    if (key.find("crate") != std::string::npos) {
        return {165, 124, 68};
    }
    return {160, 164, 170};
}

constexpr int kMapFormatVersion = 4;

std::filesystem::path brushModelPath() {
    return "assets/generated/models/crate.obj";
}

std::filesystem::path brushMaterialPath(const std::string_view materialId) {
    if (materialId == "bomb_site_a") {
        return "assets/generated/materials/bomb_site_a.mat";
    }
    if (materialId == "bomb_site_b") {
        return "assets/generated/materials/bomb_site_b.mat";
    }
    if (materialId == "floor_concrete") {
        return "assets/generated/materials/polyhaven_concrete_floor.mat";
    }
    return "assets/generated/materials/polyhaven_concrete_wall_006.mat";
}

MapProp makeBrushProp(const std::string& id,
                      const util::Vec3& position,
                      const util::Vec3& scale,
                      const std::filesystem::path& materialPath,
                      const util::Vec3& rotationDegrees = {}) {
    return MapProp{
        .id = id,
        .position = position,
        .modelPath = brushModelPath(),
        .materialPath = materialPath,
        .rotationDegrees = rotationDegrees,
        .scale = scale,
    };
}

void addPerimeterBrushes(MapData& map, const std::string& materialId, const float wallHeight) {
    constexpr float kWallThickness = 0.50f;
    const float width = static_cast<float>(map.width);
    const float depth = static_cast<float>(map.depth);
    const auto materialPath = brushMaterialPath(materialId);
    map.props.push_back(makeBrushProp("editor_brush_perimeter_west",
        {kWallThickness * 0.5f, 0.0f, depth * 0.5f},
        {kWallThickness, wallHeight, depth},
        materialPath));
    map.props.push_back(makeBrushProp("editor_brush_perimeter_east",
        {width - kWallThickness * 0.5f, 0.0f, depth * 0.5f},
        {kWallThickness, wallHeight, depth},
        materialPath));
    map.props.push_back(makeBrushProp("editor_brush_perimeter_north",
        {width * 0.5f, 0.0f, kWallThickness * 0.5f},
        {width, wallHeight, kWallThickness},
        materialPath));
    map.props.push_back(makeBrushProp("editor_brush_perimeter_south",
        {width * 0.5f, 0.0f, depth - kWallThickness * 0.5f},
        {width, wallHeight, kWallThickness},
        materialPath));
}

void addSiteMarker(MapData& map,
                   const std::string& id,
                   const util::Vec3& position,
                   const util::Vec3& scale) {
    map.props.push_back(makeBrushProp(
        id,
        position,
        scale,
        brushMaterialPath(id)));
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

util::Vec3 rotateMapPropVector(const util::Vec3& value, const util::Vec3& rotationDegrees) {
    constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;
    util::Vec3 rotated = rotateAroundZ(value, rotationDegrees.z * kDegreesToRadians);
    rotated = rotateAroundX(rotated, rotationDegrees.x * kDegreesToRadians);
    rotated = rotateAroundY(rotated, rotationDegrees.y * kDegreesToRadians);
    return rotated;
}

float propTopSurfaceHeight(const MapProp& prop) {
    const util::Vec3 half = previewPropHalfExtents(prop);
    const util::Vec3 axisX = rotateMapPropVector({1.0f, 0.0f, 0.0f}, prop.rotationDegrees);
    const util::Vec3 axisY = rotateMapPropVector({0.0f, 1.0f, 0.0f}, prop.rotationDegrees);
    const util::Vec3 axisZ = rotateMapPropVector({0.0f, 0.0f, 1.0f}, prop.rotationDegrees);
    const util::Vec3 center = {
        prop.position.x,
        prop.position.y,
        prop.position.z,
    };
    const util::Vec3 worldCenter = {
        center.x + rotateMapPropVector({0.0f, half.y, 0.0f}, prop.rotationDegrees).x,
        center.y + rotateMapPropVector({0.0f, half.y, 0.0f}, prop.rotationDegrees).y,
        center.z + rotateMapPropVector({0.0f, half.y, 0.0f}, prop.rotationDegrees).z,
    };
    return worldCenter.y +
        std::abs(axisX.y) * half.x +
        std::abs(axisY.y) * half.y +
        std::abs(axisZ.y) * half.z;
}

bool tryGetField(const simdjson::dom::object& object,
                 const char* key,
                 simdjson::dom::element& outElement) {
    return object[key].get(outElement) == simdjson::SUCCESS;
}

std::string readJsonString(const simdjson::dom::element& element,
                           const std::string& fallback = {}) {
    std::string_view value;
    if (element.get(value) == simdjson::SUCCESS) {
        return std::string(value);
    }
    return fallback;
}

int readJsonInt(const simdjson::dom::element& element, const int fallback = 0) {
    int64_t value = 0;
    if (element.get(value) == simdjson::SUCCESS) {
        return static_cast<int>(value);
    }

    double floatingValue = 0.0;
    if (element.get(floatingValue) == simdjson::SUCCESS) {
        return static_cast<int>(std::lround(floatingValue));
    }
    return fallback;
}

float readJsonFloat(const simdjson::dom::element& element, const float fallback = 0.0f) {
    double value = 0.0;
    if (element.get(value) == simdjson::SUCCESS) {
        return static_cast<float>(value);
    }

    int64_t integerValue = 0;
    if (element.get(integerValue) == simdjson::SUCCESS) {
        return static_cast<float>(integerValue);
    }
    return fallback;
}

util::Vec3 readJsonVec3(const simdjson::dom::element& element,
                        const util::Vec3 fallback = {}) {
    simdjson::dom::array array;
    if (element.get(array) != simdjson::SUCCESS) {
        return fallback;
    }

    util::Vec3 value = fallback;
    std::size_t index = 0;
    for (const auto item : array) {
        if (index == 0) value.x = readJsonFloat(item, fallback.x);
        else if (index == 1) value.y = readJsonFloat(item, fallback.y);
        else if (index == 2) value.z = readJsonFloat(item, fallback.z);
        else break;
        ++index;
    }
    return index >= 3 ? value : fallback;
}

void readPropTransformFromJson(const simdjson::dom::object& propObject, MapProp& prop) {
    simdjson::dom::element field;
    if (tryGetField(propObject, "transform", field)) {
        simdjson::dom::object transformObject;
        if (field.get(transformObject) == simdjson::SUCCESS) {
            simdjson::dom::element transformField;
            if (tryGetField(transformObject, "position", transformField)) {
                prop.position = readJsonVec3(transformField, prop.position);
            }
            if (tryGetField(transformObject, "rotation", transformField)) {
                prop.rotationDegrees = readJsonVec3(transformField, prop.rotationDegrees);
            }
            if (tryGetField(transformObject, "scale", transformField)) {
                prop.scale = readJsonVec3(transformField, prop.scale);
            }
            return;
        }
    }

    if (tryGetField(propObject, "position", field)) {
        prop.position = readJsonVec3(field, prop.position);
    }
    if (tryGetField(propObject, "rotation", field)) {
        prop.rotationDegrees = readJsonVec3(field, prop.rotationDegrees);
    }
    if (tryGetField(propObject, "scale", field)) {
        prop.scale = readJsonVec3(field, prop.scale);
    }
}

void readPropRenderFromJson(const simdjson::dom::object& propObject, MapProp& prop) {
    simdjson::dom::element field;
    if (tryGetField(propObject, "render", field)) {
        simdjson::dom::object renderObject;
        if (field.get(renderObject) == simdjson::SUCCESS) {
            simdjson::dom::element renderField;
            if (tryGetField(renderObject, "model", renderField)) {
                prop.modelPath = readJsonString(renderField);
            }
            if (tryGetField(renderObject, "material", renderField)) {
                prop.materialPath = readJsonString(renderField);
            }
            return;
        }
    }

    if (tryGetField(propObject, "model", field)) {
        prop.modelPath = readJsonString(field);
    }
    if (tryGetField(propObject, "material", field)) {
        prop.materialPath = readJsonString(field);
    }
}

MapData deserializeJsonMap(const std::string_view content) {
    MapData map;
    if (content.empty()) {
        return map;
    }

    simdjson::dom::parser parser;
    simdjson::dom::element root;
    if (parser.parse(std::string(content)).get(root) != simdjson::SUCCESS) {
        return map;
    }

    simdjson::dom::object rootObject;
    if (root.get(rootObject) != simdjson::SUCCESS) {
        return map;
    }

    simdjson::dom::element element;
    if (tryGetField(rootObject, "name", element)) {
        map.name = readJsonString(element, map.name);
    }
    if (tryGetField(rootObject, "size", element)) {
        simdjson::dom::object sizeObject;
        if (element.get(sizeObject) == simdjson::SUCCESS) {
            simdjson::dom::element sizeField;
            if (tryGetField(sizeObject, "width", sizeField)) map.width = readJsonInt(sizeField, map.width);
            if (tryGetField(sizeObject, "height", sizeField)) map.height = readJsonInt(sizeField, map.height);
            if (tryGetField(sizeObject, "depth", sizeField)) map.depth = readJsonInt(sizeField, map.depth);
        }
    }
    if (tryGetField(rootObject, "spawns", element)) {
        simdjson::dom::array spawns;
        if (element.get(spawns) == simdjson::SUCCESS) {
            for (const auto spawnElement : spawns) {
                simdjson::dom::object spawnObject;
                if (spawnElement.get(spawnObject) != simdjson::SUCCESS) {
                    continue;
                }

                SpawnPoint spawn;
                simdjson::dom::element spawnField;
                if (tryGetField(spawnObject, "team", spawnField)) spawn.team = parseTeam(readJsonString(spawnField));
                if (tryGetField(spawnObject, "position", spawnField)) spawn.position = readJsonVec3(spawnField, spawn.position);
                map.spawns.push_back(spawn);
            }
        }
    }
    if (tryGetField(rootObject, "props", element)) {
        simdjson::dom::array props;
        if (element.get(props) == simdjson::SUCCESS) {
            for (const auto propElement : props) {
                simdjson::dom::object propObject;
                if (propElement.get(propObject) != simdjson::SUCCESS) {
                    continue;
                }

                MapProp prop;
                simdjson::dom::element propField;
                if (tryGetField(propObject, "id", propField)) prop.id = readJsonString(propField, prop.id);
                readPropTransformFromJson(propObject, prop);
                readPropRenderFromJson(propObject, prop);
                map.props.push_back(std::move(prop));
            }
        }
    }
    if (tryGetField(rootObject, "lights", element)) {
        simdjson::dom::array lights;
        if (element.get(lights) == simdjson::SUCCESS) {
            for (const auto lightElement : lights) {
                simdjson::dom::object lightObject;
                if (lightElement.get(lightObject) != simdjson::SUCCESS) {
                    continue;
                }

                LightProbe light;
                simdjson::dom::element lightField;
                if (tryGetField(lightObject, "position", lightField)) light.position = readJsonVec3(lightField, light.position);
                if (tryGetField(lightObject, "color", lightField)) light.color = readJsonVec3(lightField, light.color);
                if (tryGetField(lightObject, "intensity", lightField)) light.intensity = readJsonFloat(lightField, light.intensity);
                map.lights.push_back(light);
            }
        }
    }

    return map;
}

}  // namespace

std::string MapSerializer::serialize(const MapData& map) {
    std::ostringstream out;
    out << std::setprecision(6);
    out << "{\n";
    out << "  \"format\": \"mycsg.map\",\n";
    out << "  \"version\": " << kMapFormatVersion << ",\n";
    out << "  \"name\": \"" << escapeJsonString(map.name) << "\",\n";
    out << "  \"size\": { \"width\": " << map.width << ", \"height\": " << map.height << ", \"depth\": " << map.depth << " },\n";

    out << "  \"spawns\": [\n";
    for (std::size_t index = 0; index < map.spawns.size(); ++index) {
        const auto& spawn = map.spawns[index];
        out << "    { \"team\": \"" << teamToString(spawn.team) << "\", \"position\": ";
        writeVec3Array(out, spawn.position);
        out << " }";
        out << (index + 1 < map.spawns.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    out << "  \"props\": [\n";
    for (std::size_t index = 0; index < map.props.size(); ++index) {
        const auto& prop = map.props[index];
        out << "    {\n";
        out << "      \"id\": \"" << escapeJsonString(prop.id) << "\",\n";
        out << "      \"transform\": { \"position\": ";
        writeVec3Array(out, prop.position);
        out << ", \"rotation\": ";
        writeVec3Array(out, prop.rotationDegrees);
        out << ", \"scale\": ";
        writeVec3Array(out, prop.scale);
        out << " },\n";
        out << "      \"render\": { \"model\": \"" << escapeJsonString(prop.modelPath.generic_string())
            << "\", \"material\": \"" << escapeJsonString(prop.materialPath.generic_string()) << "\" }\n";
        out << "    }";
        out << (index + 1 < map.props.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    out << "  \"lights\": [\n";
    for (std::size_t index = 0; index < map.lights.size(); ++index) {
        const auto& light = map.lights[index];
        out << "    { \"position\": ";
        writeVec3Array(out, light.position);
        out << ", \"color\": ";
        writeVec3Array(out, light.color);
        out << ", \"intensity\": " << light.intensity << " }";
        out << (index + 1 < map.lights.size() ? ",\n" : "\n");
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

MapData MapSerializer::deserialize(const std::string_view content) {
    return deserializeJsonMap(content);
}

bool MapSerializer::save(const MapData& map, const std::filesystem::path& path) {
    return util::FileSystem::writeText(path, serialize(map));
}

MapData MapSerializer::load(const std::filesystem::path& path) {
    return deserialize(util::FileSystem::readText(path));
}

MapEditor::MapEditor(MapData map) : map_(std::move(map)) {}

void MapEditor::paintFloor(const int y, const std::string& materialId) {
    constexpr float kFloorThickness = 0.10f;
    map_.props.push_back(makeBrushProp(
        "editor_brush_floor",
        {static_cast<float>(map_.width) * 0.5f, static_cast<float>(y) - kFloorThickness, static_cast<float>(map_.depth) * 0.5f},
        {static_cast<float>(map_.width), kFloorThickness, static_cast<float>(map_.depth)},
        brushMaterialPath(materialId)));
}

void MapEditor::paintPerimeterWalls(const int wallHeight, const std::string& materialId) {
    addPerimeterBrushes(map_, materialId, static_cast<float>(wallHeight));
}

void MapEditor::addCrate(const util::Vec3& position, const std::filesystem::path& modelPath, const std::filesystem::path& materialPath) {
    map_.props.push_back(MapProp{"crate", position, modelPath, materialPath});
}

void MapEditor::addSpawn(const Team team, const util::Vec3& position) {
    map_.spawns.push_back(SpawnPoint{team, position});
}

bool MapEditor::exportTopDownPreview(const std::filesystem::path& path) const {
    std::ostringstream out;
    out << "P3\n" << map_.width << ' ' << map_.depth << "\n255\n";

    for (int z = 0; z < map_.depth; ++z) {
        for (int x = 0; x < map_.width; ++x) {
            const float sampleX = static_cast<float>(x) + 0.5f;
            const float sampleZ = static_cast<float>(z) + 0.5f;

            util::ColorRgb8 color{28, 30, 36};

            bool hasProp = false;
            bool hasBarrel = false;
            util::ColorRgb8 propColor{165, 124, 68};
            for (const auto& prop : map_.props) {
                if (!previewPropHitsFootprint(prop, sampleX, sampleZ)) {
                    continue;
                }
                hasProp = true;
                const std::string key = lowerAscii(prop.id + " " + prop.modelPath.generic_string());
                hasBarrel = key.find("barrel") != std::string::npos;
                propColor = previewPropColor(prop);
                break;
            }
            const bool hasSpawn = std::ranges::any_of(map_.spawns, [x, z](const auto& spawn) {
                return static_cast<int>(std::round(spawn.position.x)) == x && static_cast<int>(std::round(spawn.position.z)) == z;
            });

            if (hasProp) color = hasBarrel ? util::ColorRgb8{148, 82, 64} : propColor;
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
    const auto classicCrateMaterial = assetRoot / "generated" / "materials" / "classic64_box_shipping_01.mat";
    MapEditor editor(MapData{
        .name = "Depot Lab",
        .width = 24,
        .height = 8,
        .depth = 24,
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
    if (util::FileSystem::exists(classicCrateMaterial)) {
        map.props.push_back(MapProp{"classic64_crate", {5.0f, 0.0f, 15.0f}, crateModel, classicCrateMaterial});
        map.props.push_back(MapProp{"classic64_crate", {5.9f, 0.0f, 15.0f}, crateModel, classicCrateMaterial});
    }
    addSiteMarker(map, "bomb_site_a", {12.0f, 0.001f, 12.0f}, {2.2f, 0.02f, 2.2f});
    addSiteMarker(map, "bomb_site_b", {19.0f, 0.001f, 7.0f}, {2.2f, 0.02f, 2.2f});
    map.lights.push_back(LightProbe{{12.0f, 6.0f, 12.0f}, {1.0f, 0.96f, 0.86f}, 9.0f});
    map.lights.push_back(LightProbe{{6.0f, 5.0f, 18.0f}, {0.8f, 0.9f, 1.0f}, 5.5f});
    return map;
}

MapData makeMetroStationShowcaseMap(const std::filesystem::path& assetRoot) {
    const auto metroModel = assetRoot / "source" / "itchio" / "metro_psx" / "Models" / "Metro.glb";
    const auto crateModel = assetRoot / "source" / "polyhaven" / "models" / "wooden_crate_02" / "wooden_crate_02_1k.gltf";
    const auto crateMaterial = assetRoot / "generated" / "materials" / "classic64_box_shipping_01.mat";
    const auto barrelModel = assetRoot / "source" / "polyhaven" / "models" / "Barrel_02" / "Barrel_02_1k.gltf";
    const auto barrelMaterial = assetRoot / "generated" / "materials" / "polyhaven_barrel_02.mat";

    MapEditor editor(MapData{
        .name = "Metro Platform",
        .width = 64,
        .height = 14,
        .depth = 112,
        .spawns = {},
        .props = {},
        .lights = {},
    });
    editor.paintFloor(0, "floor_concrete");
    editor.addSpawn(Team::Attackers, {18.0f, 1.0f, 12.0f});
    editor.addSpawn(Team::Defenders, {18.0f, 1.0f, 92.0f});

    auto& map = editor.map();
    if (util::FileSystem::exists(metroModel)) {
        map.props.push_back(MapProp{"metro_psx_station", {22.0f, -1.45f, 22.0f}, metroModel, {}});
    }
    if (util::FileSystem::exists(crateMaterial)) {
        map.props.push_back(MapProp{"classic64_crate", {14.5f, 0.0f, 18.5f}, crateModel, crateMaterial});
        map.props.push_back(MapProp{"classic64_crate", {15.4f, 0.0f, 18.5f}, crateModel, crateMaterial});
        map.props.push_back(MapProp{"classic64_crate", {16.3f, 0.0f, 18.5f}, crateModel, crateMaterial});
        map.props.push_back(MapProp{"classic64_crate", {12.8f, 0.0f, 84.2f}, crateModel, crateMaterial});
        map.props.push_back(MapProp{"classic64_crate", {13.7f, 0.0f, 84.2f}, crateModel, crateMaterial});
    }
    map.props.push_back(MapProp{"barrel_02", {12.5f, 0.0f, 22.5f}, barrelModel, barrelMaterial});
    map.props.push_back(MapProp{"barrel_02", {21.5f, 0.0f, 81.5f}, barrelModel, barrelMaterial});
    addSiteMarker(map, "bomb_site_a", {17.0f, 0.001f, 45.0f}, {2.2f, 0.02f, 2.2f});
    addSiteMarker(map, "bomb_site_b", {19.0f, 0.001f, 73.0f}, {2.2f, 0.02f, 2.2f});
    map.lights.push_back(LightProbe{{18.0f, 7.5f, 18.0f}, {1.0f, 0.95f, 0.86f}, 10.5f});
    map.lights.push_back(LightProbe{{18.0f, 7.5f, 86.0f}, {0.76f, 0.88f, 1.0f}, 9.2f});
    return map;
}

float sampleFloorHeight(const MapData& map, const float x, const float z) {
    if (x < 0.0f || z < 0.0f || x >= static_cast<float>(map.width) || z >= static_cast<float>(map.depth)) {
        return 0.0f;
    }

    float floorHeight = 0.0f;
    for (const auto& prop : map.props) {
        if (!previewPropHitsFootprint(prop, x, z)) {
            continue;
        }
        floorHeight = std::max(floorHeight, propTopSurfaceHeight(prop));
    }

    return floorHeight;
}

}  // namespace mycsg::gameplay
