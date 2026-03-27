#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mycsg::content {

struct AssetManifestEntry {
    std::string id;
    std::string displayName;
    std::filesystem::path path;
    std::string sourceLabel;
    std::string categoryLabel;
    std::string formatLabel;
    std::vector<std::string> tags;
    std::filesystem::path thumbnailPath;
    bool runtimeSupported = false;
};

struct MaterialAssetEntry {
    std::string id;
    std::string displayName;
    std::filesystem::path materialPath;
    std::filesystem::path albedoPath;
    std::string categoryLabel;
    std::vector<std::string> tags;
    std::filesystem::path thumbnailPath;
    bool generatedFromTexture = false;
};

struct AssetManifest {
    std::filesystem::path manifestPath;
    std::vector<AssetManifestEntry> models;
    std::vector<AssetManifestEntry> textures;
    std::vector<MaterialAssetEntry> materials;
    std::vector<AssetManifestEntry> editorModels;
    std::vector<MaterialAssetEntry> editorMaterials;
};

AssetManifest buildAssetManifest(const std::filesystem::path& assetRoot);
bool writeAssetManifest(const std::filesystem::path& assetRoot, const AssetManifest& manifest);

}  // namespace mycsg::content
