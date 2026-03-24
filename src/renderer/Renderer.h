#pragma once

#include "app/MainMenu.h"
#include "gameplay/MapData.h"
#include "gameplay/Simulation.h"
#include "util/MathTypes.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace mycsg::platform {
class IWindow;
}

namespace mycsg::renderer {

struct RenderFrame {
    enum class EquipmentSlot {
        Primary,
        Melee,
        Throwable,
    };

    app::AppFlow appFlow = app::AppFlow::MainMenu;
    const app::MainMenuModel* mainMenu = nullptr;
    const gameplay::SimulationWorld* world = nullptr;
    const gameplay::MapData* editingMap = nullptr;
    std::size_t selectedMenuIndex = 0;
    util::Vec3 cameraPosition{};
    float cameraYawRadians = 0.0f;
    float cameraPitchRadians = 0.0f;
    std::string activeWeaponLabel;
    int ammoInMagazine = 0;
    int reserveAmmo = 0;
    int eliminations = 0;
    bool lastShotHit = false;
    float crosshairSpread = 0.0f;
    float recoilKick = 0.0f;
    float muzzleFlash = 0.0f;
    EquipmentSlot activeEquipmentSlot = EquipmentSlot::Primary;
    std::string meleeWeaponLabel;
    std::string selectedThrowableLabel;
    std::string activeOpticLabel;
    std::filesystem::path activeEquipmentModelPath;
    std::filesystem::path activeEquipmentAlbedoPath;
    std::filesystem::path activeEquipmentMaterialPath;
    std::string activeEquipmentDisplayLabel;
    int fragCount = 0;
    int flashCount = 0;
    int smokeCount = 0;
    float flashOverlay = 0.0f;
    float smokeOverlay = 0.0f;
    float opticMagnification = 1.0f;
    std::size_t selectedSettingsIndex = 0;
    float settingsMouseSensitivity = 0.0f;
    float settingsMouseVerticalSensitivity = 0.0f;
    float settingsMaxLookPitchDegrees = 0.0f;
    bool settingsAutoReload = false;
    int editorCursorX = 0;
    int editorCursorZ = 0;
    std::string editorToolLabel;
    std::string editorStatusLabel;
    std::string editorMapFileLabel;
    std::size_t editorMapIndex = 0;
    std::size_t editorMapCount = 0;
    std::string mapBrowserTitle;
    std::string mapBrowserSubtitle;
    std::string mapBrowserStatus;
    std::vector<std::string> mapBrowserItems;
    std::size_t mapBrowserSelectedIndex = 0;
    std::string multiplayerMapLabel;
    std::string multiplayerSessionTypeLabel;
    std::string multiplayerEndpointLabel;
    int multiplayerMaxPlayers = 0;
    std::size_t multiplayerSelectedIndex = 0;
    std::string multiplayerStatusLabel;
    bool multiplayerSessionActive = false;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize(platform::IWindow& window) = 0;
    virtual void render(const RenderFrame& frame) = 0;
    virtual void shutdown() = 0;
};

std::unique_ptr<IRenderer> createRenderer();

}  // namespace mycsg::renderer
