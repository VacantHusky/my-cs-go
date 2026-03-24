#include "app/Settings.h"

#include "util/FileSystem.h"

#include <sstream>

namespace mycsg::app {

namespace {

std::string serialize(const Settings& settings) {
    std::ostringstream out;
    out << "# my-cs-go settings\n";
    out << "video.width=" << settings.video.width << '\n';
    out << "video.height=" << settings.video.height << '\n';
    out << "video.fullscreen=" << (settings.video.fullscreen ? 1 : 0) << '\n';
    out << "video.render_scale=" << settings.video.renderScale << '\n';
    out << "video.antialiasing=" << settings.video.antialiasing << '\n';
    out << "audio.master=" << settings.audio.master << '\n';
    out << "audio.music=" << settings.audio.music << '\n';
    out << "audio.effects=" << settings.audio.effects << '\n';
    out << "audio.voice_chat=" << (settings.audio.voiceChat ? 1 : 0) << '\n';
    out << "gameplay.mouse_sensitivity=" << settings.gameplay.mouseSensitivity << '\n';
    out << "gameplay.mouse_vertical_sensitivity=" << settings.gameplay.mouseVerticalSensitivity << '\n';
    out << "gameplay.max_look_pitch_degrees=" << settings.gameplay.maxLookPitchDegrees << '\n';
    out << "gameplay.hold_to_aim=" << (settings.gameplay.holdToAim ? 1 : 0) << '\n';
    out << "gameplay.auto_reload=" << (settings.gameplay.autoReload ? 1 : 0) << '\n';
    out << "network.player_name=" << settings.network.playerName << '\n';
    out << "network.host=" << settings.network.defaultServerHost << '\n';
    out << "network.port=" << settings.network.port << '\n';
    out << "network.max_players=" << settings.network.maxPlayers << '\n';
    return out.str();
}

void applyKeyValue(Settings& settings, const std::string& key, const std::string& value) {
    if (key == "video.width") settings.video.width = std::stoi(value);
    else if (key == "video.height") settings.video.height = std::stoi(value);
    else if (key == "video.fullscreen") settings.video.fullscreen = value == "1";
    else if (key == "video.render_scale") settings.video.renderScale = std::stof(value);
    else if (key == "video.antialiasing") settings.video.antialiasing = value;
    else if (key == "audio.master") settings.audio.master = std::stof(value);
    else if (key == "audio.music") settings.audio.music = std::stof(value);
    else if (key == "audio.effects") settings.audio.effects = std::stof(value);
    else if (key == "audio.voice_chat") settings.audio.voiceChat = value == "1";
    else if (key == "gameplay.mouse_sensitivity") settings.gameplay.mouseSensitivity = std::stof(value);
    else if (key == "gameplay.mouse_vertical_sensitivity") settings.gameplay.mouseVerticalSensitivity = std::stof(value);
    else if (key == "gameplay.max_look_pitch_degrees") settings.gameplay.maxLookPitchDegrees = std::stof(value);
    else if (key == "gameplay.hold_to_aim") settings.gameplay.holdToAim = value == "1";
    else if (key == "gameplay.auto_reload") settings.gameplay.autoReload = value == "1";
    else if (key == "network.player_name") settings.network.playerName = value;
    else if (key == "network.host") settings.network.defaultServerHost = value;
    else if (key == "network.port") settings.network.port = static_cast<std::uint16_t>(std::stoi(value));
    else if (key == "network.max_players") settings.network.maxPlayers = std::stoi(value);
}

}  // namespace

Settings loadSettings(const std::filesystem::path& path) {
    Settings settings;
    const std::string content = util::FileSystem::readText(path);
    if (content.empty()) {
        return settings;
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        applyKeyValue(settings, line.substr(0, separator), line.substr(separator + 1));
    }

    return settings;
}

bool saveSettings(const Settings& settings, const std::filesystem::path& path) {
    return util::FileSystem::writeText(path, serialize(settings));
}

}  // namespace mycsg::app
