#include "content/ObjectCatalog.h"

#include "util/FileSystem.h"

#include <simdjson.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace mycsg::content {

namespace {

constexpr std::string_view kObjectCatalogPath = "generated/object_catalog.json";

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
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

const AssetManifestEntry* findModelAssetByPath(const AssetManifest& manifest, const std::filesystem::path& relativePath) {
    const std::string needle = relativePath.lexically_normal().generic_string();
    const auto it = std::find_if(manifest.models.begin(), manifest.models.end(), [&needle](const AssetManifestEntry& entry) {
        return entry.path.lexically_normal().generic_string() == needle;
    });
    return it != manifest.models.end() ? &*it : nullptr;
}

const MaterialAssetEntry* findMaterialAssetByPath(const AssetManifest& manifest, const std::filesystem::path& relativePath) {
    const std::string needle = relativePath.lexically_normal().generic_string();
    const auto it = std::find_if(manifest.materials.begin(), manifest.materials.end(), [&needle](const MaterialAssetEntry& entry) {
        return entry.materialPath.lexically_normal().generic_string() == needle;
    });
    return it != manifest.materials.end() ? &*it : nullptr;
}

std::filesystem::path chooseThumbnailPath(const AssetManifest& manifest,
                                          const std::filesystem::path& modelPath,
                                          const std::filesystem::path& materialPath) {
    if (const auto* material = findMaterialAssetByPath(manifest, materialPath); material != nullptr && !material->thumbnailPath.empty()) {
        return material->thumbnailPath;
    }
    if (const auto* model = findModelAssetByPath(manifest, modelPath); model != nullptr && !model->thumbnailPath.empty()) {
        return model->thumbnailPath;
    }
    return {};
}

ObjectAssetDefinition makeObjectDefinition(const AssetManifest& manifest,
                                           const std::string& id,
                                           const std::string& label,
                                           const std::string& category,
                                           const ObjectPlacementKind placementKind,
                                           const std::filesystem::path& modelPath,
                                           const std::filesystem::path& materialPath,
                                           const util::Vec3 collisionHalfExtents,
                                           const util::Vec3 collisionCenterOffset,
                                           const util::ColorRgb8 previewColor,
                                           const bool cylindricalFootprint,
                                           const bool editorVisible,
                                           std::vector<std::string> tags) {
    return ObjectAssetDefinition{
        .id = id,
        .label = label,
        .category = category,
        .placementKind = placementKind,
        .modelPath = modelPath,
        .materialPath = materialPath,
        .thumbnailPath = chooseThumbnailPath(manifest, modelPath, materialPath),
        .collisionHalfExtents = collisionHalfExtents,
        .collisionCenterOffset = collisionCenterOffset,
        .previewColor = previewColor,
        .cylindricalFootprint = cylindricalFootprint,
        .editorVisible = editorVisible,
        .tags = std::move(tags),
    };
}

std::string placementKindToString(const ObjectPlacementKind placementKind) {
    switch (placementKind) {
        case ObjectPlacementKind::Prop: return "prop";
        case ObjectPlacementKind::Wall: return "wall";
    }
    return "prop";
}

ObjectPlacementKind parsePlacementKind(const std::string_view value) {
    return lowerAscii(std::string(value)) == "wall" ? ObjectPlacementKind::Wall : ObjectPlacementKind::Prop;
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

bool readJsonBool(const simdjson::dom::element& element, const bool fallback) {
    bool value = false;
    if (element.get(value) == simdjson::SUCCESS) {
        return value;
    }
    return fallback;
}

int readJsonInt(const simdjson::dom::element& element, const int fallback = 0) {
    int64_t value = 0;
    if (element.get(value) == simdjson::SUCCESS) {
        return static_cast<int>(value);
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

util::ColorRgb8 readJsonColor(const simdjson::dom::element& element,
                              const util::ColorRgb8 fallback = {}) {
    simdjson::dom::array array;
    if (element.get(array) != simdjson::SUCCESS) {
        return fallback;
    }

    util::ColorRgb8 value = fallback;
    std::size_t index = 0;
    for (const auto item : array) {
        const int component = std::clamp(readJsonInt(item, 0), 0, 255);
        if (index == 0) value.r = static_cast<std::uint8_t>(component);
        else if (index == 1) value.g = static_cast<std::uint8_t>(component);
        else if (index == 2) value.b = static_cast<std::uint8_t>(component);
        else break;
        ++index;
    }
    return index >= 3 ? value : fallback;
}

std::vector<std::string> readTags(const simdjson::dom::element& element) {
    std::vector<std::string> tags;
    simdjson::dom::array array;
    if (element.get(array) != simdjson::SUCCESS) {
        return tags;
    }
    for (const auto item : array) {
        tags.push_back(readJsonString(item));
    }
    return tags;
}

void writeVec3(std::ostringstream& out, const util::Vec3& value) {
    out << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

void writeColor(std::ostringstream& out, const util::ColorRgb8& value) {
    out << '['
        << static_cast<int>(value.r) << ", "
        << static_cast<int>(value.g) << ", "
        << static_cast<int>(value.b) << ']';
}

}  // namespace

const ObjectAssetDefinition* ObjectCatalog::find(const std::string_view id) const {
    const auto it = std::find_if(objects.begin(), objects.end(), [id](const ObjectAssetDefinition& object) {
        return object.id == id;
    });
    return it != objects.end() ? &*it : nullptr;
}

bool ObjectCatalog::upsert(ObjectAssetDefinition definition) {
    if (definition.id.empty()) {
        return false;
    }
    const auto it = std::find_if(objects.begin(), objects.end(), [&definition](const ObjectAssetDefinition& object) {
        return object.id == definition.id;
    });
    if (it != objects.end()) {
        *it = std::move(definition);
    } else {
        objects.push_back(std::move(definition));
    }
    std::sort(objects.begin(), objects.end(), [](const ObjectAssetDefinition& lhs, const ObjectAssetDefinition& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        if (lhs.label != rhs.label) {
            return lhs.label < rhs.label;
        }
        return lhs.id < rhs.id;
    });
    return true;
}

bool ObjectCatalog::remove(const std::string_view id) {
    const auto before = objects.size();
    std::erase_if(objects, [id](const ObjectAssetDefinition& object) {
        return object.id == id;
    });
    return before != objects.size();
}

std::size_t ObjectCatalog::categoryCount() const {
    std::set<std::string> categories;
    for (const auto& object : objects) {
        if (!object.editorVisible) {
            continue;
        }
        categories.insert(object.category);
    }
    return categories.size();
}

ObjectCatalog buildDefaultObjectCatalog(const std::filesystem::path& assetRoot, const AssetManifest& manifest) {
    (void)assetRoot;

    ObjectCatalog catalog;
    catalog.catalogPath = kObjectCatalogPath;

    const auto add = [&](ObjectAssetDefinition definition) {
        catalog.upsert(std::move(definition));
    };

    add(makeObjectDefinition(
        manifest,
        "editor_brush_floor",
        "地面盒体",
        "结构",
        ObjectPlacementKind::Wall,
        "generated/models/crate.obj",
        "generated/materials/polyhaven_concrete_floor.mat",
        {0.50f, 0.50f, 0.50f},
        {0.0f, 0.50f, 0.0f},
        {92, 100, 84},
        false,
        false,
        {"brush", "floor", "default"}));
    add(makeObjectDefinition(
        manifest,
        "editor_brush_wall",
        "盒体墙",
        "结构",
        ObjectPlacementKind::Wall,
        "generated/models/crate.obj",
        "generated/materials/polyhaven_concrete_wall_006.mat",
        {0.50f, 0.50f, 0.50f},
        {0.0f, 0.50f, 0.0f},
        {112, 114, 118},
        false,
        true,
        {"brush", "wall", "cover"}));
    add(makeObjectDefinition(
        manifest,
        "bomb_site_a",
        "A 爆点标记",
        "标记",
        ObjectPlacementKind::Prop,
        "generated/models/crate.obj",
        "generated/materials/bomb_site_a.mat",
        {0.50f, 0.50f, 0.50f},
        {0.0f, 0.50f, 0.0f},
        {174, 72, 56},
        false,
        true,
        {"marker", "bomb-site"}));
    add(makeObjectDefinition(
        manifest,
        "bomb_site_b",
        "B 爆点标记",
        "标记",
        ObjectPlacementKind::Prop,
        "generated/models/crate.obj",
        "generated/materials/bomb_site_b.mat",
        {0.50f, 0.50f, 0.50f},
        {0.0f, 0.50f, 0.0f},
        {62, 96, 172},
        false,
        true,
        {"marker", "bomb-site"}));
    add(makeObjectDefinition(
        manifest,
        "wooden_crate",
        "木箱",
        "掩体",
        ObjectPlacementKind::Prop,
        "source/polyhaven/models/wooden_crate_02/wooden_crate_02_1k.gltf",
        "generated/materials/polyhaven_wooden_crate_02.mat",
        {0.48f, 0.48f, 0.48f},
        {0.0f, 0.48f, 0.0f},
        {165, 124, 68},
        false,
        true,
        {"crate", "cover", "polyhaven"}));
    add(makeObjectDefinition(
        manifest,
        "classic64_crate",
        "Classic64 货运箱",
        "掩体",
        ObjectPlacementKind::Prop,
        "source/polyhaven/models/wooden_crate_02/wooden_crate_02_1k.gltf",
        "generated/materials/classic64_box_shipping_01.mat",
        {0.48f, 0.48f, 0.48f},
        {0.0f, 0.48f, 0.0f},
        {172, 136, 80},
        false,
        true,
        {"crate", "cover", "classic64"}));
    add(makeObjectDefinition(
        manifest,
        "barrel_02",
        "金属油桶",
        "掩体",
        ObjectPlacementKind::Prop,
        "source/polyhaven/models/Barrel_02/Barrel_02_1k.gltf",
        "generated/materials/polyhaven_barrel_02.mat",
        {0.34f, 0.55f, 0.34f},
        {0.0f, 0.48f, 0.0f},
        {148, 82, 64},
        true,
        true,
        {"barrel", "cover", "polyhaven"}));
    add(makeObjectDefinition(
        manifest,
        "metro_psx_station",
        "Metro 站台结构",
        "场景",
        ObjectPlacementKind::Prop,
        "source/itchio/metro_psx/Models/Metro.glb",
        {},
        {5.0f, 2.5f, 5.0f},
        {0.0f, 1.0f, 0.0f},
        {120, 126, 134},
        false,
        true,
        {"metro", "scene", "large"}));

    return catalog;
}

ObjectCatalog loadObjectCatalog(const std::filesystem::path& assetRoot, const std::filesystem::path& path) {
    ObjectCatalog catalog;
    catalog.catalogPath = std::filesystem::relative(path, assetRoot);

    const std::string content = util::FileSystem::readText(path);
    if (content.empty()) {
        return catalog;
    }

    simdjson::dom::parser parser;
    simdjson::dom::element root;
    if (parser.parse(content).get(root) != simdjson::SUCCESS) {
        return catalog;
    }

    simdjson::dom::object rootObject;
    if (root.get(rootObject) != simdjson::SUCCESS) {
        return catalog;
    }

    simdjson::dom::element field;
    if (tryGetField(rootObject, "catalog", field)) {
        const std::string loadedPath = readJsonString(field);
        if (!loadedPath.empty()) {
            catalog.catalogPath = loadedPath;
        }
    }

    if (!tryGetField(rootObject, "objects", field)) {
        return catalog;
    }

    simdjson::dom::array objects;
    if (field.get(objects) != simdjson::SUCCESS) {
        return catalog;
    }

    for (const auto objectElement : objects) {
        simdjson::dom::object object;
        if (objectElement.get(object) != simdjson::SUCCESS) {
            continue;
        }

        ObjectAssetDefinition definition;
        simdjson::dom::element objectField;
        if (tryGetField(object, "id", objectField)) definition.id = readJsonString(objectField);
        if (definition.id.empty()) {
            continue;
        }
        if (tryGetField(object, "label", objectField)) definition.label = readJsonString(objectField, definition.id);
        if (tryGetField(object, "category", objectField)) definition.category = readJsonString(objectField);
        if (tryGetField(object, "placement", objectField)) definition.placementKind = parsePlacementKind(readJsonString(objectField));
        if (tryGetField(object, "modelPath", objectField)) definition.modelPath = readJsonString(objectField);
        if (tryGetField(object, "materialPath", objectField)) definition.materialPath = readJsonString(objectField);
        if (tryGetField(object, "thumbnailPath", objectField)) definition.thumbnailPath = readJsonString(objectField);
        if (tryGetField(object, "collisionHalfExtents", objectField)) {
            definition.collisionHalfExtents = readJsonVec3(objectField, definition.collisionHalfExtents);
        }
        if (tryGetField(object, "collisionCenterOffset", objectField)) {
            definition.collisionCenterOffset = readJsonVec3(objectField, definition.collisionCenterOffset);
        }
        if (tryGetField(object, "previewColor", objectField)) {
            definition.previewColor = readJsonColor(objectField, definition.previewColor);
        }
        if (tryGetField(object, "cylindricalFootprint", objectField)) {
            definition.cylindricalFootprint = readJsonBool(objectField, definition.cylindricalFootprint);
        }
        if (tryGetField(object, "editorVisible", objectField)) {
            definition.editorVisible = readJsonBool(objectField, definition.editorVisible);
        }
        if (tryGetField(object, "tags", objectField)) {
            definition.tags = readTags(objectField);
        }
        catalog.upsert(std::move(definition));
    }

    return catalog;
}

bool writeObjectCatalog(const std::filesystem::path& assetRoot, const ObjectCatalog& catalog) {
    std::ostringstream out;
    out << std::setprecision(6);
    out << "{\n";
    out << "  \"format\": \"mycsg.object_catalog\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"catalog\": \"" << escapeJsonString(catalog.catalogPath.generic_string()) << "\",\n";
    out << "  \"objects\": [\n";
    for (std::size_t index = 0; index < catalog.objects.size(); ++index) {
        const auto& object = catalog.objects[index];
        out << "    {\n";
        out << "      \"id\": \"" << escapeJsonString(object.id) << "\",\n";
        out << "      \"label\": \"" << escapeJsonString(object.label) << "\",\n";
        out << "      \"category\": \"" << escapeJsonString(object.category) << "\",\n";
        out << "      \"placement\": \"" << placementKindToString(object.placementKind) << "\",\n";
        out << "      \"modelPath\": \"" << escapeJsonString(object.modelPath.generic_string()) << "\",\n";
        out << "      \"materialPath\": \"" << escapeJsonString(object.materialPath.generic_string()) << "\",\n";
        out << "      \"thumbnailPath\": \"" << escapeJsonString(object.thumbnailPath.generic_string()) << "\",\n";
        out << "      \"collisionHalfExtents\": ";
        writeVec3(out, object.collisionHalfExtents);
        out << ",\n";
        out << "      \"collisionCenterOffset\": ";
        writeVec3(out, object.collisionCenterOffset);
        out << ",\n";
        out << "      \"previewColor\": ";
        writeColor(out, object.previewColor);
        out << ",\n";
        out << "      \"cylindricalFootprint\": " << (object.cylindricalFootprint ? "true" : "false") << ",\n";
        out << "      \"editorVisible\": " << (object.editorVisible ? "true" : "false") << ",\n";
        out << "      \"tags\": [";
        for (std::size_t tagIndex = 0; tagIndex < object.tags.size(); ++tagIndex) {
            out << '"' << escapeJsonString(object.tags[tagIndex]) << '"';
            if (tagIndex + 1 < object.tags.size()) {
                out << ", ";
            }
        }
        out << "]\n";
        out << "    }";
        out << (index + 1 < catalog.objects.size() ? ",\n" : "\n");
    }
    out << "  ]\n";
    out << "}\n";
    return util::FileSystem::writeText(assetRoot / catalog.catalogPath, out.str());
}

}  // namespace mycsg::content
