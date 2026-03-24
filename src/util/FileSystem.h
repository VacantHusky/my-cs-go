#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mycsg::util {

class FileSystem {
public:
    static bool ensureDirectory(const std::filesystem::path& path);
    static bool writeText(const std::filesystem::path& path, std::string_view content);
    static bool writeBinary(const std::filesystem::path& path, const std::vector<std::byte>& bytes);
    static std::string readText(const std::filesystem::path& path);
    static std::vector<std::byte> readBinary(const std::filesystem::path& path);
    static bool exists(const std::filesystem::path& path);
};

}  // namespace mycsg::util
