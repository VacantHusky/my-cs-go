#pragma once

#include "app/MainMenu.h"
#include "app/Settings.h"
#include "content/GameContent.h"
#include "gameplay/MapData.h"
#include "gameplay/Simulation.h"
#include "network/Network.h"
#include "platform/Window.h"

#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace mycsg::renderer {
class IRenderer;
}

namespace mycsg::app {

enum class TrainingEquipmentSlot {
    Primary,
    Melee,
    Throwable,
};

enum class MapEditorTool {
    Wall,
    Crate,
    AttackerSpawn,
    DefenderSpawn,
    Eraser,
};

class Application {
public:
    Application();
    ~Application();

    int run();

private:
    bool initialize();
    void tick(float deltaSeconds);
    void shutdown();
    void bootstrapProjectFiles();
    void handleInput();
    void navigateMenu(int delta);
    void navigateSettings(int delta);
    void adjustSelectedSetting(int delta);
    void activateSelectedSetting();
    void activateSelectedMenuItem();
    void activateMenuItem(std::size_t index);
    void returnToMainMenu();
    void initializeSinglePlayerView();
    void updateSinglePlayerView(const platform::InputSnapshot& input, float deltaSeconds);
    void syncInputMode();
    bool collidesWithWorld(const util::Vec3& position) const;
    const content::WeaponDefinition* findWeaponDefinition(const std::string& id) const;
    void switchToNextTrainingWeapon();
    void reloadTrainingWeapon();
    void fireTrainingWeapon();
    void selectTrainingEquipmentSlot(TrainingEquipmentSlot slot);
    void cycleTrainingThrowable();
    void cycleTrainingOptic();
    void useTrainingThrowable();
    void useTrainingMelee();
    void handleMapEditorInput(const platform::InputSnapshot& input);
    void handleMapBrowserInput(const platform::InputSnapshot& input);
    void handleMultiplayerLobbyInput(const platform::InputSnapshot& input);
    void moveMapEditorCursor(int dx, int dz);
    bool selectMapEditorCellFromMouse(int mouseX, int mouseY);
    void applyMapEditorTool();
    void eraseMapEditorCell();
    void openMapBrowser(AppFlow targetFlow);
    void activateSelectedMapBrowserItem();
    void navigateMultiplayerLobby(int delta);
    void adjustSelectedMultiplayerSetting(int delta);
    void activateSelectedMultiplayerSetting();
    void restartNetworkSession(network::SessionType type, const char* reason);
    void refreshMapCatalog();
    void loadEditorMapByIndex(std::size_t index);
    void cycleEditorMap(int delta);
    void createNewEditorMap();
    gameplay::MapData makeBlankEditorMap(const std::string& name) const;
    std::filesystem::path nextCustomMapPath() const;
    void saveActiveMapArtifacts(const char* reason);
    const char* mapEditorToolLabel() const;
    void persistSettings(const char* reason);
    bool lineOfSightBlocked(const util::Vec3& from, const util::Vec3& to) const;
    void refreshWindowTitle();
    void logCurrentFlow() const;
    std::size_t hitTestMainMenuItem(int mouseX, int mouseY) const;
    std::size_t hitTestMapBrowserItem(int mouseX, int mouseY) const;

    std::filesystem::path projectRoot_ = ".";
    std::filesystem::path assetRoot_ = "assets";
    std::filesystem::path settingsPath_ = "settings.cfg";
    Settings settings_;
    MainMenuModel mainMenu_;
    content::ContentDatabase contentDatabase_;
    gameplay::MapData activeMap_;
    gameplay::SimulationWorld simulation_;
    network::NetworkSession networkSession_;
    std::unique_ptr<platform::IWindow> window_;
    std::unique_ptr<renderer::IRenderer> renderer_;
    AppFlow currentFlow_ = AppFlow::MainMenu;
    AppFlow mapBrowserTargetFlow_ = AppFlow::SinglePlayerLobby;
    std::size_t selectedMenuIndex_ = 0;
    std::size_t selectedSettingsIndex_ = 0;
    bool needsRedraw_ = true;
    std::size_t selectedMultiplayerIndex_ = 0;
    platform::InputSnapshot lastInput_{};
    util::Vec3 singlePlayerCameraPosition_{};
    float singlePlayerCameraYawRadians_ = 0.0f;
    float singlePlayerCameraPitchRadians_ = 0.0f;
    float singlePlayerJumpOffset_ = 0.0f;
    float singlePlayerVerticalVelocity_ = 0.0f;
    std::string activeWeaponLabel_ = "AK-12";
    std::vector<std::string> trainingWeaponIds_;
    std::size_t activeTrainingWeaponIndex_ = 0;
    int ammoInMagazine_ = 30;
    int reserveAmmo_ = 90;
    int eliminations_ = 0;
    float fireCooldownSeconds_ = 0.0f;
    float hitFlashSeconds_ = 0.0f;
    float muzzleFlashSeconds_ = 0.0f;
    float flashOverlaySeconds_ = 0.0f;
    float smokeOverlaySeconds_ = 0.0f;
    float crosshairSpreadDegrees_ = 0.0f;
    float aimYawOffsetRadians_ = 0.0f;
    float viewKickAmount_ = 0.0f;
    TrainingEquipmentSlot activeTrainingSlot_ = TrainingEquipmentSlot::Primary;
    content::OpticType activeOptic_ = content::OpticType::RedDot;
    std::string meleeWeaponLabel_ = "战术匕首";
    std::size_t selectedThrowableIndex_ = 0;
    int fragCount_ = 2;
    int flashCount_ = 2;
    int smokeCount_ = 2;
    MapEditorTool mapEditorTool_ = MapEditorTool::Wall;
    int mapEditorCursorX_ = 3;
    int mapEditorCursorZ_ = 3;
    std::string mapEditorStatus_ = "可编辑";
    network::SessionType multiplayerSessionType_ = network::SessionType::Host;
    bool multiplayerSessionActive_ = false;
    std::string multiplayerStatus_ = "尚未启动房间";
    std::vector<std::filesystem::path> mapCatalogPaths_;
    std::size_t activeMapCatalogIndex_ = 0;
    std::filesystem::path activeMapPath_ = "assets/maps/depot_lab.arena";
    std::minstd_rand recoilRandom_{1337u};
};

}  // namespace mycsg::app
