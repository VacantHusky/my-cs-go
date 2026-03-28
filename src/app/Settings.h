#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace mycsg::app {

struct VideoSettings {
    int width = 1600;
    int height = 900;
    bool fullscreen = false;
    float renderScale = 1.0f;
    std::string antialiasing = "TAA";
    float editorSidebarWidth = 520.0f;
};

struct AudioSettings {
    float master = 0.85f;
    float music = 0.25f;
    float effects = 1.0f;
    bool voiceChat = true;
};

struct GameplaySettings {
    float mouseSensitivity = 0.9f;
    float mouseVerticalSensitivity = 1.4f;
    float maxLookPitchDegrees = 77.0f;
    bool holdToAim = true;
    bool autoReload = true;
};

struct NetworkSettings {
    std::string playerName = "Tiger";
    std::string defaultServerHost = "127.0.0.1";
    std::uint16_t port = 37015;
    int maxPlayers = 10;
};

struct Settings {
    VideoSettings video;
    AudioSettings audio;
    GameplaySettings gameplay;
    NetworkSettings network;
};

Settings loadSettings(const std::filesystem::path& path);
bool saveSettings(const Settings& settings, const std::filesystem::path& path);

}  // namespace mycsg::app
