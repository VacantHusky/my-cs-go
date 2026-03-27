#pragma once

#include "content/AssetManifest.h"
#include "util/MathTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mycsg::content {

enum class ObjectPlacementKind {
    Prop,
    Wall,
};

struct ObjectAssetDefinition {
    std::string id;
    std::string label;
    std::string category;
    ObjectPlacementKind placementKind = ObjectPlacementKind::Prop;
    std::filesystem::path modelPath;
    std::filesystem::path materialPath;
    std::filesystem::path thumbnailPath;
    util::Vec3 collisionHalfExtents{0.42f, 0.52f, 0.42f};
    util::Vec3 collisionCenterOffset{0.0f, 0.52f, 0.0f};
    util::ColorRgb8 previewColor{160, 164, 170};
    bool cylindricalFootprint = false;
    bool editorVisible = true;
    std::vector<std::string> tags;
};

struct ObjectCatalog {
    std::filesystem::path catalogPath;
    std::vector<ObjectAssetDefinition> objects;

    const ObjectAssetDefinition* find(std::string_view id) const;
    bool upsert(ObjectAssetDefinition definition);
    bool remove(std::string_view id);
    std::size_t categoryCount() const;
};

ObjectCatalog buildDefaultObjectCatalog(const std::filesystem::path& assetRoot, const AssetManifest& manifest);
ObjectCatalog loadObjectCatalog(const std::filesystem::path& assetRoot, const std::filesystem::path& path);
bool writeObjectCatalog(const std::filesystem::path& assetRoot, const ObjectCatalog& catalog);

}  // namespace mycsg::content
