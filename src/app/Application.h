#pragma once

#include "app/MainMenu.h"
#include "app/Settings.h"
#include "audio/AudioSystem.h"
#include "content/GameContent.h"
#include "gameplay/MapData.h"
#include "gameplay/PhysicsWorld.h"
#include "gameplay/Simulation.h"
#include "network/Network.h"
#include "platform/Window.h"

#include <filesystem>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
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
    Select,
    Place,
    Erase,
};

enum class MapEditorPlacementKind {
    Wall,
    Prop,
    AttackerSpawn,
    DefenderSpawn,
};

enum class MapEditorViewMode {
    Perspective,
    Ortho25D,
};

enum class ApplicationLaunchMode {
    Game,
    Editor,
};

class Application {
public:
    explicit Application(ApplicationLaunchMode launchMode = ApplicationLaunchMode::Game);
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
    void returnToMainMenu(std::string_view statusOverride = {});
    void initializeSinglePlayerView();
    void initializeMapEditorView();
    void initializeLaunchFlow();
    void updateSinglePlayerView(const platform::InputSnapshot& input, float deltaSeconds);
    void updateMapEditorView(const platform::InputSnapshot& input, float deltaSeconds);
    gameplay::PlayerState buildNetworkLocalPlayerState() const;
    void syncLocalPlayerSimulationState();
    void applyLatestNetworkMapState();
    void applyLatestNetworkSnapshot();
    void handleMovementFeedback(float baseSpread, float maxSpread);
    void handlePendingDetonations();
    void syncInputMode();
    bool isAuthoritativeGameplaySession() const;
    bool isRemoteClientSession() const;
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
    void syncMapEditorTargetFromView();
    std::optional<std::size_t> pickMapEditorPropFromView(float* outDistance = nullptr) const;
    std::optional<std::size_t> pickMapEditorSpawnFromView(float* outDistance = nullptr) const;
    bool pickMapEditorCellFromView(int& cellX, int& cellZ, float* outDistance = nullptr) const;
    std::optional<std::size_t> findMapEditorPropIndexAtCell(int cellX, int cellZ) const;
    void syncSelectedMapEditorPropFromCursor();
    gameplay::MapProp* selectedMapEditorProp();
    const gameplay::MapProp* selectedMapEditorProp() const;
    void setSelectedMapEditorPropPosition(const util::Vec3& position);
    void setSelectedMapEditorPropRotation(const util::Vec3& rotationDegrees);
    void setSelectedMapEditorPropScale(const util::Vec3& scale);
    bool buildMapEditorRay(util::Vec3& origin, util::Vec3& direction) const;
    gameplay::MapProp buildMapEditorPlacementPreviewProp() const;
    gameplay::SpawnPoint buildMapEditorPlacementPreviewSpawn() const;
    void cycleMapEditorPlacementKind(int delta);
    void pushMapEditorUndoSnapshot(const char* reason);
    bool restoreMapEditorUndoSnapshot();
    void clearMapEditorUndoHistory();
    void clampMapEditorAssetSelection();
    std::vector<const content::ObjectAssetDefinition*> mapEditorSelectableObjects() const;
    const content::ObjectAssetDefinition* selectedMapEditorObjectAsset() const;
    void clampManagedObjectAssetSelection();
    const content::ObjectAssetDefinition* selectedManagedObjectAsset() const;
    std::size_t findObjectAssetIndexById(std::string_view id) const;
    std::string makeNextObjectAssetId(std::string_view seed) const;
    int countObjectAssetReferencesInMap(const gameplay::MapData& map, std::string_view id) const;
    int countObjectAssetReferencesInStoredMaps(std::string_view id) const;
    void createManagedObjectAsset();
    void saveManagedObjectAsset(const content::ObjectAssetDefinition& definition, std::string_view previousId);
    void deleteManagedObjectAsset();
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
    void switchMapEditorViewMode(MapEditorViewMode nextMode);
    void syncMapEditorCameraState();
    util::Vec3 clampMapEditorPerspectiveCameraPosition(const util::Vec3& position) const;
    util::Vec3 clampMapEditorOrthoFocusPosition(const util::Vec3& position) const;
    util::Vec3 deriveMapEditorFocusPointFromPerspective() const;
    const char* mapEditorToolLabel() const;
    const char* mapEditorPlacementKindLabel() const;
    const char* mapEditorViewModeLabel() const;
    void persistSettings(const char* reason);
    bool lineOfSightBlocked(const util::Vec3& from, const util::Vec3& to) const;
    void refreshWindowTitle();
    void logCurrentFlow() const;
    void handleRendererUiActions();
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
    gameplay::PhysicsWorld physicsWorld_;
    network::NetworkSession networkSession_;
    audio::AudioSystem audioSystem_;
    std::unique_ptr<platform::IWindow> window_;
    std::unique_ptr<renderer::IRenderer> renderer_;
    ApplicationLaunchMode launchMode_ = ApplicationLaunchMode::Game;
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
    util::Vec3 mapEditorCameraPosition_{};
    float mapEditorCameraYawRadians_ = 0.0f;
    float mapEditorCameraPitchRadians_ = 0.0f;
    util::Vec3 mapEditorPerspectiveCameraPosition_{};
    float mapEditorPerspectiveCameraYawRadians_ = 0.0f;
    float mapEditorPerspectiveCameraPitchRadians_ = 0.0f;
    util::Vec3 mapEditorPerspectiveFocusOffset_{};
    util::Vec3 mapEditorOrthoFocusPosition_{};
    util::Vec3 mapEditorTargetPosition_{};
    util::Vec3 mapEditorTargetNormal_{0.0f, 1.0f, 0.0f};
    bool mapEditorHasTarget_ = false;
    bool mapEditorTargetOnSurface_ = false;
    bool mapEditorMouseLookActive_ = false;
    bool mapEditorShowMeshOutline_ = true;
    bool mapEditorShowCollisionOutline_ = true;
    bool mapEditorShowBoundingBox_ = false;
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
    MapEditorTool mapEditorTool_ = MapEditorTool::Select;
    MapEditorPlacementKind mapEditorPlacementKind_ = MapEditorPlacementKind::Prop;
    MapEditorViewMode mapEditorViewMode_ = MapEditorViewMode::Perspective;
    std::size_t editorObjectAssetIndex_ = 0;
    std::size_t managedObjectAssetIndex_ = 0;
    float editorPropRotationDegrees_ = 0.0f;
    std::size_t editorPropScalePresetIndex_ = 1;
    float mapEditorOrthoSpan_ = 14.0f;
    int mapEditorCursorX_ = 3;
    int mapEditorCursorZ_ = 3;
    std::optional<std::size_t> hoveredEditorPropIndex_;
    std::optional<std::size_t> hoveredEditorSpawnIndex_;
    std::optional<std::size_t> selectedEditorPropIndex_;
    std::vector<gameplay::MapData> mapEditorUndoStack_;
    std::string mapEditorStatus_ = "可编辑";
    network::SessionType multiplayerSessionType_ = network::SessionType::Host;
    bool multiplayerSessionActive_ = false;
    bool multiplayerGameplayReady_ = true;
    bool receivedMultiplayerSnapshot_ = false;
    std::uint64_t appliedNetworkMapRevision_ = 0;
    std::string multiplayerStatus_ = "尚未启动房间";
    std::vector<std::filesystem::path> mapCatalogPaths_;
    std::size_t activeMapCatalogIndex_ = 0;
    std::filesystem::path activeMapPath_ = "assets/maps/depot_lab.arena";
    std::minstd_rand recoilRandom_{1337u};
};

}  // namespace mycsg::app
