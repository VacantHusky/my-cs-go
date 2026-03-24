#include "util/FileSystem.h"

#include <fstream>
#include <iterator>

namespace mycsg::util {

bool FileSystem::ensureDirectory(const std::filesystem::path& path) {
    if (path.empty()) {
        return true;
    }
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return !error;
    }
    return std::filesystem::create_directories(path, error);
}

bool FileSystem::writeText(const std::filesystem::path& path, std::string_view content) {
    if (!ensureDirectory(path.parent_path())) {
        return false;
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(stream);
}

bool FileSystem::writeBinary(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    if (!ensureDirectory(path.parent_path())) {
        return false;
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

std::string FileSystem::readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> FileSystem::readBinary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::vector<char> buffer{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(buffer.size());
    for (std::size_t index = 0; index < buffer.size(); ++index) {
        bytes[index] = static_cast<std::byte>(static_cast<unsigned char>(buffer[index]));
    }
    return bytes;
}

bool FileSystem::exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

}  // namespace mycsg::util
