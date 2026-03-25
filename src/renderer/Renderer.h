#pragma once

#include "app/MainMenu.h"
#include "gameplay/MapData.h"
#include "gameplay/Simulation.h"
#include "util/MathTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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

    enum class EditorPlacementPreviewKind {
        None,
        Prop,
        Spawn,
    };

    app::AppFlow appFlow = app::AppFlow::MainMenu;
    const app::MainMenuModel* mainMenu = nullptr;
    const gameplay::SimulationWorld* world = nullptr;
    const gameplay::MapData* editingMap = nullptr;
    std::string localPlayerId;
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
    std::filesystem::path playerCharacterModelPath;
    std::filesystem::path playerCharacterAlbedoPath;
    std::filesystem::path playerCharacterMaterialPath;
    float playerCharacterScale = 1.0f;
    float playerCharacterYawOffsetRadians = 0.0f;
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
    bool editorHasTarget = false;
    bool editorTargetOnSurface = false;
    util::Vec3 editorTargetPosition{};
    util::Vec3 editorTargetNormal{0.0f, 1.0f, 0.0f};
    int editorCursorX = 0;
    int editorCursorZ = 0;
    std::string editorToolLabel;
    std::string editorPlacementKindLabel;
    std::string editorViewModeLabel;
    std::string editorStatusLabel;
    std::string editorMapFileLabel;
    bool editorMouseLookActive = false;
    bool editorIsOrthoView = false;
    float editorOrthoSpan = 0.0f;
    bool editorUndoAvailable = false;
    std::size_t editorMapIndex = 0;
    std::size_t editorMapCount = 0;
    std::string editorWallMaterialLabel;
    std::string editorPropPresetLabel;
    std::string editorCellFloorLabel;
    std::string editorCellCoverLabel;
    std::string editorCellPropLabel;
    std::string editorCellSpawnLabel;
    int editorPropCount = 0;
    int editorSpawnCount = 0;
    int hoveredEditorPropIndex = -1;
    int hoveredEditorSpawnIndex = -1;
    int eraseEditorPropIndex = -1;
    int eraseEditorSpawnIndex = -1;
    EditorPlacementPreviewKind editorPlacementPreviewKind = EditorPlacementPreviewKind::None;
    gameplay::MapProp editorPlacementPreviewProp{};
    gameplay::SpawnPoint editorPlacementPreviewSpawn{};
    int selectedEditorPropIndex = -1;
    bool hasSelectedEditorProp = false;
    std::string selectedEditorPropLabel;
    std::string selectedEditorPropModelLabel;
    std::string selectedEditorPropMaterialLabel;
    util::Vec3 selectedEditorPropPosition{};
    util::Vec3 selectedEditorPropRotationDegrees{};
    util::Vec3 selectedEditorPropScale{1.0f, 1.0f, 1.0f};
    std::string mapBrowserTitle;
    std::string mapBrowserSubtitle;
    std::string mapBrowserStatus;
    std::vector<std::string> mapBrowserItems;
    std::size_t mapBrowserSelectedIndex = 0;
    std::string multiplayerMapLabel;
    std::string multiplayerSessionTypeLabel;
    int multiplayerSessionTypeIndex = 0;
    std::string multiplayerHost;
    int multiplayerPort = 0;
    std::string multiplayerEndpointLabel;
    int multiplayerMaxPlayers = 0;
    std::size_t multiplayerSelectedIndex = 0;
    std::string multiplayerStatusLabel;
    bool multiplayerSessionActive = false;
};

enum class UiActionType {
    ActivateMainMenuItem,
    SelectMapBrowserItem,
    ActivateCurrentMapBrowserItem,
    CycleMapBrowser,
    CreateMapBrowserMap,
    SelectMapEditorTool,
    SelectMapEditorPlacementKind,
    SelectEditorWallMaterial,
    SelectEditorPropPreset,
    ToggleMapEditorProjection,
    UndoMapEditorChange,
    SetSelectedEditorPropPosition,
    SetSelectedEditorPropRotation,
    SetSelectedEditorPropScale,
    MoveMapEditorCursor,
    ApplyMapEditorTool,
    EraseMapEditorCell,
    CycleEditorMap,
    CreateEditorMap,
    SaveEditorMap,
    AdjustMultiplayerSetting,
    SetMultiplayerSessionType,
    SetMultiplayerHost,
    SetMultiplayerPort,
    SetMultiplayerMaxPlayers,
    ActivateMultiplayerSetting,
    AdjustSetting,
    ActivateSetting,
    ReturnToMainMenu,
};

struct UiAction {
    UiActionType type = UiActionType::ActivateMainMenuItem;
    std::int32_t value0 = 0;
    std::int32_t value1 = 0;
    std::string text;
    util::Vec3 vectorValue{};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize(platform::IWindow& window) = 0;
    virtual void render(const RenderFrame& frame) = 0;
    virtual bool wantsKeyboardCapture() const = 0;
    virtual bool wantsMouseCapture() const = 0;
    virtual std::vector<UiAction> consumeUiActions() = 0;
    virtual void shutdown() = 0;
};

std::unique_ptr<IRenderer> createRenderer();

}  // namespace mycsg::renderer
