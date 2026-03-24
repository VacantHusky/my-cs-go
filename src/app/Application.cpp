#include "app/Application.h"

#include "platform/Window.h"
#include "renderer/Renderer.h"
#include "util/FileSystem.h"
#include "util/Log.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <thread>

namespace mycsg::app {

namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kSinglePlayerEyeHeight = 1.0f;
constexpr float kSinglePlayerJumpVelocity = 5.2f;
constexpr float kSinglePlayerGravity = 14.0f;
std::filesystem::path assetRootPath() {
#ifdef _WIN32
    return "assets";
#elif defined(MYCSGO_ASSET_ROOT)
    return MYCSGO_ASSET_ROOT;
#else
    return "assets";
#endif
}

float degreesToRadians(const float degrees) {
    return degrees * (kPi / 180.0f);
}

float approachZero(const float value, const float amount) {
    if (value > 0.0f) {
        return std::max(0.0f, value - amount);
    }
    if (value < 0.0f) {
        return std::min(0.0f, value + amount);
    }
    return 0.0f;
}

float randomSigned(std::minstd_rand& generator) {
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    return distribution(generator);
}

float trainingBaseSpread(const content::WeaponDefinition& weapon) {
    return std::max(0.18f, weapon.aimSpread * 0.72f);
}

constexpr std::size_t kSettingsEntryCount = 4;
constexpr std::size_t kMultiplayerEntryCount = 4;

const char* settingToggleLabel(const bool enabled) {
    return enabled ? "开启" : "关闭";
}

const char* sessionTypeLabel(const network::SessionType type) {
    switch (type) {
        case network::SessionType::Offline:
            return "离线";
        case network::SessionType::Host:
            return "主机";
        case network::SessionType::Client:
            return "客户端";
    }
    return "主机";
}

struct WeaponHandlingProfile {
    float shotSpreadKick = 0.6f;
    float moveSpreadGain = 2.4f;
    float spreadRecovery = 4.5f;
    float recoilReturnSpeed = 10.0f;
    float yawKickScale = 1.0f;
    float viewKickScale = 1.0f;
    float maxSpread = 5.0f;
    float targetRadius = 0.30f;
    float effectiveRange = 28.0f;
    float muzzleFlashDuration = 0.05f;
};

WeaponHandlingProfile makeHandlingProfile(const content::WeaponDefinition& weapon) {
    WeaponHandlingProfile profile{};
    profile.maxSpread = std::max(trainingBaseSpread(weapon) + 1.4f, weapon.hipSpread * 1.9f);

    switch (weapon.category) {
        case content::WeaponCategory::Rifle:
            profile.shotSpreadKick = 0.34f + weapon.hipSpread * 0.14f;
            profile.moveSpreadGain = 2.1f;
            profile.spreadRecovery = 4.8f;
            profile.recoilReturnSpeed = 10.5f;
            profile.yawKickScale = 1.0f;
            profile.viewKickScale = 0.95f;
            profile.targetRadius = 0.28f;
            profile.effectiveRange = 34.0f;
            profile.muzzleFlashDuration = 0.05f;
            break;
        case content::WeaponCategory::SniperRifle:
            profile.shotSpreadKick = 0.55f + weapon.hipSpread * 0.10f;
            profile.moveSpreadGain = 4.2f;
            profile.spreadRecovery = 5.8f;
            profile.recoilReturnSpeed = 8.2f;
            profile.yawKickScale = 0.72f;
            profile.viewKickScale = 1.45f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 3.4f, weapon.hipSpread * 1.35f);
            profile.targetRadius = 0.18f;
            profile.effectiveRange = 48.0f;
            profile.muzzleFlashDuration = 0.08f;
            break;
        case content::WeaponCategory::SubmachineGun:
            profile.shotSpreadKick = 0.26f + weapon.hipSpread * 0.18f;
            profile.moveSpreadGain = 3.0f;
            profile.spreadRecovery = 5.4f;
            profile.recoilReturnSpeed = 11.2f;
            profile.yawKickScale = 0.9f;
            profile.viewKickScale = 0.70f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 2.2f, weapon.hipSpread * 2.25f);
            profile.targetRadius = 0.33f;
            profile.effectiveRange = 24.0f;
            profile.muzzleFlashDuration = 0.045f;
            break;
        case content::WeaponCategory::Shotgun:
            profile.shotSpreadKick = 0.82f + weapon.hipSpread * 0.12f;
            profile.moveSpreadGain = 3.8f;
            profile.spreadRecovery = 4.1f;
            profile.recoilReturnSpeed = 8.6f;
            profile.yawKickScale = 1.2f;
            profile.viewKickScale = 1.25f;
            profile.maxSpread = std::max(trainingBaseSpread(weapon) + 2.8f, weapon.hipSpread * 1.75f);
            profile.targetRadius = 0.72f;
            profile.effectiveRange = 14.0f;
            profile.muzzleFlashDuration = 0.07f;
            break;
        case content::WeaponCategory::Melee:
        case content::WeaponCategory::Grenade:
            break;
    }

    return profile;
}

const char* opticLabel(const content::OpticType optic) {
    switch (optic) {
        case content::OpticType::IronSight:
            return "机瞄";
        case content::OpticType::RedDot:
            return "红点";
        case content::OpticType::X2:
            return "2倍";
        case content::OpticType::X4:
            return "4倍";
        case content::OpticType::X8:
            return "8倍";
    }
    return "红点";
}

float opticMagnification(const content::OpticType optic) {
    switch (optic) {
        case content::OpticType::IronSight:
        case content::OpticType::RedDot:
            return 1.0f;
        case content::OpticType::X2:
            return 2.0f;
        case content::OpticType::X4:
            return 4.0f;
        case content::OpticType::X8:
            return 8.0f;
    }
    return 1.0f;
}

std::string throwableLabel(const std::size_t index) {
    switch (index % 3) {
        case 0:
            return "破片手雷";
        case 1:
            return "闪光弹";
        default:
            return "烟雾弹";
    }
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

util::Vec3 centerOfCell(const int cellX, const int cellZ, const float y = 0.0f) {
    return {static_cast<float>(cellX) + 0.5f, y, static_cast<float>(cellZ) + 0.5f};
}

bool positionInsideCell(const util::Vec3& position, const int cellX, const int cellZ) {
    return static_cast<int>(std::floor(position.x)) == cellX &&
           static_cast<int>(std::floor(position.z)) == cellZ;
}

bool pointHitsPropFootprint(const gameplay::MapProp& prop, const float x, const float z) {
    const std::string key = lowerAscii(prop.id + " " + prop.modelPath.generic_string());
    const float dx = std::abs(x - prop.position.x);
    const float dz = std::abs(z - prop.position.z);

    if (key.find("barrel") != std::string::npos) {
        constexpr float kBarrelRadius = 0.30f;
        return dx * dx + dz * dz <= kBarrelRadius * kBarrelRadius;
    }

    if (key.find("crate") != std::string::npos) {
        constexpr float kCrateHalfExtent = 0.42f;
        return dx <= kCrateHalfExtent && dz <= kCrateHalfExtent;
    }

    constexpr float kGenericHalfExtent = 0.34f;
    return dx <= kGenericHalfExtent && dz <= kGenericHalfExtent;
}

renderer::RenderFrame::EquipmentSlot renderEquipmentSlot(const TrainingEquipmentSlot slot) {
    switch (slot) {
        case TrainingEquipmentSlot::Primary:
            return renderer::RenderFrame::EquipmentSlot::Primary;
        case TrainingEquipmentSlot::Melee:
            return renderer::RenderFrame::EquipmentSlot::Melee;
        case TrainingEquipmentSlot::Throwable:
            return renderer::RenderFrame::EquipmentSlot::Throwable;
    }
    return renderer::RenderFrame::EquipmentSlot::Primary;
}

}  // namespace

Application::Application() = default;
Application::~Application() = default;

int Application::run() {
    if (!initialize()) {
        shutdown();
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto previous = clock::now();

    while (!window_->shouldClose()) {
        const auto now = clock::now();
        const std::chrono::duration<float> delta = now - previous;
        previous = now;

        window_->pollEvents();
        tick(delta.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    shutdown();
    return 0;
}

bool Application::initialize() {
    spdlog::info("[Init] 正在加载设置与资源目录...");
    assetRoot_ = assetRootPath();
    settingsPath_ = projectRoot_ / "settings.cfg";

    settings_ = loadSettings(settingsPath_);
    saveSettings(settings_, settingsPath_);

    spdlog::info("[Init] 正在引导项目资源...");
    bootstrapProjectFiles();
    spdlog::info("[Init] 正在创建默认场景...");
    activeMap_ = gameplay::makeDefaultBombDefusalMap(assetRoot_);
    spdlog::info("[Init] 场景创建完毕: {} ({}x{})", activeMap_.name, activeMap_.width, activeMap_.depth);

    spdlog::info("[Init] 正在创建离线模拟世界...");
    simulation_ = gameplay::makeOfflinePracticeWorld(activeMap_);
    spdlog::info("[Init] 离线模拟世界创建完毕，当前角色数: {}", simulation_.players().size());
    networkSession_ = network::NetworkSession({
        .type = network::SessionType::Offline,
        .endpoint = {settings_.network.defaultServerHost, settings_.network.port},
    });

    spdlog::info("[Init] 正在创建窗口...");
    window_ = platform::createWindow();
    if (!window_ || !window_->create({settings_.video.width, settings_.video.height, mainMenu_.title()})) {
        spdlog::error("Failed to create game window.");
        return false;
    }
    syncInputMode();

    spdlog::info("[Init] 正在初始化渲染器...");
    renderer_ = renderer::createRenderer();
    if (!renderer_ || !renderer_->initialize(*window_)) {
        spdlog::error("Failed to initialize renderer.");
        return false;
    }

    spdlog::info("[Init] 正在启动网络会话...");
    if (!networkSession_.start()) {
        spdlog::error("Failed to initialize networking session.");
        return false;
    }

    spdlog::info("Loaded {} weapons and {} material profiles.",
        contentDatabase_.weapons().size(), contentDatabase_.materials().size());
    initializeSinglePlayerView();
    refreshWindowTitle();
    logCurrentFlow();
    return true;
}

void Application::tick(const float deltaSeconds) {
    handleInput();
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        updateSinglePlayerView(lastInput_, deltaSeconds);
        simulation_.tick(deltaSeconds);
        networkSession_.update(simulation_);
        needsRedraw_ = true;
    }

    if (!needsRedraw_) {
        return;
    }

    const content::WeaponDefinition* previewWeapon = nullptr;
    std::string previewLabel = activeWeaponLabel_;
    switch (activeTrainingSlot_) {
        case TrainingEquipmentSlot::Primary:
            if (!trainingWeaponIds_.empty()) {
                previewWeapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
            }
            previewLabel = activeWeaponLabel_;
            break;
        case TrainingEquipmentSlot::Melee:
            previewWeapon = findWeaponDefinition("combat_knife");
            previewLabel = meleeWeaponLabel_;
            break;
        case TrainingEquipmentSlot::Throwable:
            if (selectedThrowableIndex_ % 3 == 0) {
                previewWeapon = findWeaponDefinition("frag");
            } else if (selectedThrowableIndex_ % 3 == 1) {
                previewWeapon = findWeaponDefinition("flashbang");
            } else {
                previewWeapon = findWeaponDefinition("smoke");
            }
            previewLabel = throwableLabel(selectedThrowableIndex_);
            break;
    }

    renderer::RenderFrame frame{
        .appFlow = currentFlow_,
        .mainMenu = currentFlow_ == AppFlow::MainMenu ? &mainMenu_ : nullptr,
        .world = &simulation_,
        .editingMap = currentFlow_ == AppFlow::MapEditor ? &activeMap_ : nullptr,
        .selectedMenuIndex = selectedMenuIndex_,
        .cameraPosition = singlePlayerCameraPosition_,
        .cameraYawRadians = singlePlayerCameraYawRadians_ + aimYawOffsetRadians_,
        .cameraPitchRadians = singlePlayerCameraPitchRadians_,
        .activeWeaponLabel = activeWeaponLabel_,
        .ammoInMagazine = ammoInMagazine_,
        .reserveAmmo = reserveAmmo_,
        .eliminations = eliminations_,
        .lastShotHit = hitFlashSeconds_ > 0.0f,
        .crosshairSpread = crosshairSpreadDegrees_,
        .recoilKick = viewKickAmount_,
        .muzzleFlash = muzzleFlashSeconds_,
        .activeEquipmentSlot = renderEquipmentSlot(activeTrainingSlot_),
        .meleeWeaponLabel = meleeWeaponLabel_,
        .selectedThrowableLabel = throwableLabel(selectedThrowableIndex_),
        .activeOpticLabel = opticLabel(activeOptic_),
        .activeEquipmentModelPath = previewWeapon != nullptr ? previewWeapon->assets.modelPath : std::filesystem::path{},
        .activeEquipmentAlbedoPath = previewWeapon != nullptr ? previewWeapon->assets.albedoPath : std::filesystem::path{},
        .activeEquipmentMaterialPath = previewWeapon != nullptr ? previewWeapon->assets.materialPath : std::filesystem::path{},
        .activeEquipmentDisplayLabel = previewLabel,
        .fragCount = fragCount_,
        .flashCount = flashCount_,
        .smokeCount = smokeCount_,
        .flashOverlay = flashOverlaySeconds_,
        .smokeOverlay = smokeOverlaySeconds_,
        .opticMagnification = opticMagnification(activeOptic_),
        .selectedSettingsIndex = selectedSettingsIndex_,
        .settingsMouseSensitivity = settings_.gameplay.mouseSensitivity,
        .settingsMouseVerticalSensitivity = settings_.gameplay.mouseVerticalSensitivity,
        .settingsMaxLookPitchDegrees = settings_.gameplay.maxLookPitchDegrees,
        .settingsAutoReload = settings_.gameplay.autoReload,
        .editorCursorX = mapEditorCursorX_,
        .editorCursorZ = mapEditorCursorZ_,
        .editorToolLabel = mapEditorToolLabel(),
        .editorStatusLabel = mapEditorStatus_,
        .editorMapFileLabel = activeMapPath_.stem().string(),
        .editorMapIndex = activeMapCatalogIndex_,
        .editorMapCount = mapCatalogPaths_.size(),
        .mapBrowserTitle = mapBrowserTargetFlow_ == AppFlow::MapEditor ? "选择要编辑的地图" : "选择训练地图",
        .mapBrowserSubtitle = mapBrowserTargetFlow_ == AppFlow::MapEditor
            ? "同一套地图资源会被单机模式与编辑器共同使用。"
            : (mapBrowserTargetFlow_ == AppFlow::MultiPlayerLobby
                ? "先选地图，再进入联机房间参数页。"
                : "选择一张地图后进入本地训练场或机器人对战。"),
        .mapBrowserStatus = mapEditorStatus_,
        .mapBrowserItems = {},
        .mapBrowserSelectedIndex = activeMapCatalogIndex_,
        .multiplayerMapLabel = activeMap_.name,
        .multiplayerSessionTypeLabel = sessionTypeLabel(multiplayerSessionType_),
        .multiplayerEndpointLabel = settings_.network.defaultServerHost + ":" + std::to_string(settings_.network.port),
        .multiplayerMaxPlayers = settings_.network.maxPlayers,
        .multiplayerSelectedIndex = selectedMultiplayerIndex_,
        .multiplayerStatusLabel = multiplayerStatus_,
        .multiplayerSessionActive = multiplayerSessionActive_,
    };
    frame.mapBrowserItems.reserve(mapCatalogPaths_.size());
    for (const auto& path : mapCatalogPaths_) {
        frame.mapBrowserItems.push_back(path.stem().string());
    }
    renderer_->render(frame);
    needsRedraw_ = false;
}

void Application::shutdown() {
    networkSession_.stop();
    if (renderer_) {
        renderer_->shutdown();
    }
}

void Application::bootstrapProjectFiles() {
    util::FileSystem::ensureDirectory(assetRoot_ / "generated");
    util::FileSystem::ensureDirectory(assetRoot_ / "maps");
    contentDatabase_.bootstrap(assetRoot_);

    spdlog::info("[Init] 正在导出场景文件与预览图...");
    activeMap_ = gameplay::makeDefaultBombDefusalMap(assetRoot_);
    activeMapPath_ = assetRoot_ / "maps" / "depot_lab.arena";
    saveActiveMapArtifacts("初始化默认场景");
    refreshMapCatalog();
    spdlog::info("[Init] 场景文件导出完毕: assets/maps/depot_lab.arena");
}

void Application::handleInput() {
    lastInput_ = window_->consumeInput();
    const platform::InputSnapshot& input = lastInput_;

    if (currentFlow_ == AppFlow::MainMenu) {
        if (input.primaryClickPressed) {
            activateMenuItem(hitTestMainMenuItem(input.mouseX, input.mouseY));
        }
        if (input.upPressed || input.leftPressed) {
            navigateMenu(-1);
        }
        if (input.downPressed || input.rightPressed) {
            navigateMenu(1);
        }
        if (input.confirmPressed) {
            activateSelectedMenuItem();
        }
        if (input.backPressed) {
            window_->requestClose();
        }
        return;
    }

    if (input.backPressed) {
        returnToMainMenu();
        return;
    }

    if (currentFlow_ == AppFlow::MapBrowser) {
        handleMapBrowserInput(input);
    } else if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        if (input.selectPrimaryPressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Primary);
        }
        if (input.selectMeleePressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Melee);
        }
        if (input.selectThrowablePressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Throwable);
        }
        if (input.cycleThrowablePressed) {
            cycleTrainingThrowable();
        }
        if (input.cycleOpticPressed) {
            cycleTrainingOptic();
        }
        if (input.primaryClickPressed || input.firePressed) {
            switch (activeTrainingSlot_) {
                case TrainingEquipmentSlot::Primary:
                    fireTrainingWeapon();
                    break;
                case TrainingEquipmentSlot::Melee:
                    useTrainingMelee();
                    break;
                case TrainingEquipmentSlot::Throwable:
                    useTrainingThrowable();
                    break;
            }
        }
        if (input.jumpPressed && singlePlayerJumpOffset_ <= 0.001f) {
            singlePlayerVerticalVelocity_ = kSinglePlayerJumpVelocity;
        }
        if (input.reloadPressed && activeTrainingSlot_ == TrainingEquipmentSlot::Primary) {
            reloadTrainingWeapon();
        }
        if (input.switchWeaponPressed) {
            selectTrainingEquipmentSlot(TrainingEquipmentSlot::Primary);
            switchToNextTrainingWeapon();
        }
        if (input.firePressed) {
            spdlog::info("[SinglePlayer] local practice world ready, Esc 返回主菜单。");
        }
    } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
        handleMultiplayerLobbyInput(input);
    } else if (currentFlow_ == AppFlow::MapEditor) {
        handleMapEditorInput(input);
    } else if (currentFlow_ == AppFlow::Settings) {
        if (input.upPressed) {
            navigateSettings(-1);
        }
        if (input.downPressed) {
            navigateSettings(1);
        }
        if (input.leftPressed) {
            adjustSelectedSetting(-1);
        }
        if (input.rightPressed) {
            adjustSelectedSetting(1);
        }
        if (input.confirmPressed) {
            activateSelectedSetting();
        }
    }
}

void Application::navigateMenu(const int delta) {
    const auto& items = mainMenu_.items();
    if (items.empty()) {
        return;
    }

    const int count = static_cast<int>(items.size());
    const int current = static_cast<int>(selectedMenuIndex_);
    selectedMenuIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    refreshWindowTitle();
    needsRedraw_ = true;
}

void Application::navigateSettings(const int delta) {
    const int count = static_cast<int>(kSettingsEntryCount);
    const int current = static_cast<int>(selectedSettingsIndex_);
    selectedSettingsIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    refreshWindowTitle();
    needsRedraw_ = true;
}

void Application::adjustSelectedSetting(const int delta) {
    bool changed = false;
    switch (selectedSettingsIndex_) {
        case 0: {
            const float next = std::clamp(
                std::round((settings_.gameplay.mouseSensitivity + static_cast<float>(delta) * 0.1f) * 10.0f) / 10.0f,
                0.1f, 3.0f);
            if (std::abs(next - settings_.gameplay.mouseSensitivity) > 0.001f) {
                settings_.gameplay.mouseSensitivity = next;
                changed = true;
            }
            break;
        }
        case 1: {
            const float next = std::clamp(
                std::round((settings_.gameplay.mouseVerticalSensitivity + static_cast<float>(delta) * 0.1f) * 10.0f) / 10.0f,
                0.5f, 3.0f);
            if (std::abs(next - settings_.gameplay.mouseVerticalSensitivity) > 0.001f) {
                settings_.gameplay.mouseVerticalSensitivity = next;
                changed = true;
            }
            break;
        }
        case 2: {
            const float next = std::clamp(
                std::round(settings_.gameplay.maxLookPitchDegrees + static_cast<float>(delta) * 2.0f),
                45.0f, 88.0f);
            if (std::abs(next - settings_.gameplay.maxLookPitchDegrees) > 0.001f) {
                settings_.gameplay.maxLookPitchDegrees = next;
                changed = true;
            }
            break;
        }
        case 3: {
            const bool next = delta > 0;
            if (next != settings_.gameplay.autoReload) {
                settings_.gameplay.autoReload = next;
                changed = true;
            }
            break;
        }
        default:
            break;
    }

    if (changed) {
        persistSettings("设置项已更新");
    }
}

void Application::activateSelectedSetting() {
    if (selectedSettingsIndex_ == 3) {
        settings_.gameplay.autoReload = !settings_.gameplay.autoReload;
        persistSettings("自动换弹已切换");
        return;
    }

    adjustSelectedSetting(1);
}

void Application::activateSelectedMenuItem() {
    activateMenuItem(selectedMenuIndex_);
}

void Application::activateMenuItem(const std::size_t index) {
    const MenuItem* item = mainMenu_.itemAt(index);
    if (item == nullptr) {
        return;
    }

    selectedMenuIndex_ = index;

    if (item->target == AppFlow::Exit) {
        window_->requestClose();
        return;
    }

    if (item->target == AppFlow::SinglePlayerLobby ||
        item->target == AppFlow::MapEditor ||
        item->target == AppFlow::MultiPlayerLobby) {
        openMapBrowser(item->target);
        return;
    }

    currentFlow_ = item->target;
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        initializeSinglePlayerView();
    } else if (currentFlow_ == AppFlow::MapEditor) {
        refreshMapCatalog();
        mapEditorCursorX_ = std::clamp(mapEditorCursorX_, 0, std::max(0, activeMap_.width - 1));
        mapEditorCursorZ_ = std::clamp(mapEditorCursorZ_, 0, std::max(0, activeMap_.depth - 1));
        mapEditorStatus_ = "地图编辑器已就绪";
    } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
        selectedMultiplayerIndex_ = 0;
        multiplayerStatus_ = "已进入房间参数页";
    } else if (currentFlow_ == AppFlow::Settings) {
        selectedSettingsIndex_ = 0;
    }
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::returnToMainMenu() {
    currentFlow_ = AppFlow::MainMenu;
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::refreshWindowTitle() {
    const MenuItem* item = mainMenu_.itemAt(selectedMenuIndex_);
    std::string title = mainMenu_.title();
    title += " | ";

    if (currentFlow_ == AppFlow::MainMenu) {
        title += "菜单";
        if (item != nullptr) {
            title += " | 当前选中: ";
            title += item->label;
            title += " | 方向键/WASD 切换, Enter 确认, Esc 退出";
        }
    } else {
        switch (currentFlow_) {
            case AppFlow::MapBrowser: title += "地图选择"; break;
            case AppFlow::SinglePlayerLobby: title += "单机模式"; break;
            case AppFlow::MultiPlayerLobby: title += "联机模式"; break;
            case AppFlow::MapEditor: title += "地图编辑器"; break;
            case AppFlow::Settings: title += "设置"; break;
            case AppFlow::Exit: title += "退出"; break;
            case AppFlow::MainMenu: title += "菜单"; break;
        }
        if (currentFlow_ == AppFlow::Settings) {
            title += " | W/S 选择, A/D 调整, Enter 切换, Esc 返回";
        } else if (currentFlow_ == AppFlow::MapBrowser) {
            title += " | W/S 选择地图, Enter 确认, Q/E 切换, F6 新建, Esc 返回";
        } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
            title += " | W/S 选择项目, A/D 调整, Enter 启动/切换, Esc 返回";
        } else if (currentFlow_ == AppFlow::MapEditor) {
            title += " | WASD/方向键移动光标, Q/E切图, F6新建, 回车放置, Delete删除, F5保存, Esc返回";
        } else {
            title += " | Esc 返回主菜单";
        }
    }

    window_->setTitle(title);
}

void Application::syncInputMode() {
    if (window_ != nullptr) {
        window_->setRelativeMouseMode(currentFlow_ == AppFlow::SinglePlayerLobby);
    }
}

void Application::logCurrentFlow() const {
    switch (currentFlow_) {
        case AppFlow::MainMenu:
            spdlog::info("[UI] Main menu ready. Use Arrow Keys/WASD + Enter.");
            break;
        case AppFlow::MapBrowser:
            spdlog::info("[UI] Entered MapBrowser. Use W/S, Q/E, Enter, F6.");
            break;
        case AppFlow::SinglePlayerLobby:
            spdlog::info("[UI] Entered SinglePlayerLobby.");
            break;
        case AppFlow::MultiPlayerLobby:
            spdlog::info("[UI] Entered MultiPlayerLobby. Use W/S/A/D + Enter to configure room.");
            break;
        case AppFlow::MapEditor:
            spdlog::info("[UI] Entered MapEditor. Use WASD/方向键移动, Q/E切图, F6新建, Enter放置, G循环工具, F5保存.");
            break;
        case AppFlow::Settings:
            spdlog::info("[UI] Entered Settings. Use W/S to select, A/D to adjust, Enter to toggle.");
            break;
        case AppFlow::Exit:
            spdlog::info("[UI] Exit requested.");
            break;
    }
}

void Application::initializeSinglePlayerView() {
    spdlog::info("[SinglePlayer] 正在创建训练场相机与出生点...");
    simulation_ = gameplay::makeOfflinePracticeWorld(activeMap_);
    singlePlayerCameraPosition_ = {3.0f, 1.0f, 3.0f};
    if (!simulation_.players().empty()) {
        singlePlayerCameraPosition_ = simulation_.players().front().position;
    } else {
        for (const auto& spawn : activeMap_.spawns) {
            if (spawn.team == gameplay::Team::Attackers) {
                singlePlayerCameraPosition_ = spawn.position;
                break;
            }
        }
    }

    singlePlayerCameraPosition_.x += 0.2f;
    singlePlayerCameraPosition_.z += 0.2f;
    singlePlayerCameraYawRadians_ = 0.15f;
    singlePlayerCameraPitchRadians_ = 0.0f;
    singlePlayerJumpOffset_ = 0.0f;
    singlePlayerVerticalVelocity_ = 0.0f;
    singlePlayerCameraPosition_.y = gameplay::sampleFloorHeight(activeMap_, singlePlayerCameraPosition_.x, singlePlayerCameraPosition_.z) +
        kSinglePlayerEyeHeight;

    trainingWeaponIds_.clear();
    spdlog::info("[SinglePlayer] 正在装配可用武器列表...");
    for (const auto& weapon : contentDatabase_.weapons()) {
        using content::WeaponCategory;
        if (weapon.category == WeaponCategory::Rifle ||
            weapon.category == WeaponCategory::SniperRifle ||
            weapon.category == WeaponCategory::SubmachineGun ||
            weapon.category == WeaponCategory::Shotgun) {
            trainingWeaponIds_.push_back(weapon.id);
        }
    }
    if (trainingWeaponIds_.empty()) {
        trainingWeaponIds_.push_back("ak12");
    }

    activeTrainingWeaponIndex_ = 0;
    fireCooldownSeconds_ = 0.0f;
    hitFlashSeconds_ = 0.0f;
    muzzleFlashSeconds_ = 0.0f;
    flashOverlaySeconds_ = 0.0f;
    smokeOverlaySeconds_ = 0.0f;
    aimYawOffsetRadians_ = 0.0f;
    viewKickAmount_ = 0.0f;
    eliminations_ = 0;
    activeTrainingSlot_ = TrainingEquipmentSlot::Primary;
    activeOptic_ = content::OpticType::RedDot;
    selectedThrowableIndex_ = 0;
    fragCount_ = 2;
    flashCount_ = 2;
    smokeCount_ = 2;
    if (const auto* weapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_])) {
        activeWeaponLabel_ = weapon->displayName;
        ammoInMagazine_ = weapon->magazineSize;
        reserveAmmo_ = weapon->reserveAmmo;
        crosshairSpreadDegrees_ = trainingBaseSpread(*weapon);
    }
    spdlog::info("[SinglePlayer] 训练场准备完毕，初始武器: {}，出生点: ({}, {}, {})",
        activeWeaponLabel_,
        singlePlayerCameraPosition_.x,
        singlePlayerCameraPosition_.y,
        singlePlayerCameraPosition_.z);
}

void Application::updateSinglePlayerView(const platform::InputSnapshot& input, const float deltaSeconds) {
    constexpr float turnSpeed = 1.8f;
    constexpr float moveSpeed = 4.0f;
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    const content::WeaponDefinition fallbackWeapon{};
    const content::WeaponDefinition& activeWeapon = weapon != nullptr ? *weapon : fallbackWeapon;
    const WeaponHandlingProfile handling = makeHandlingProfile(activeWeapon);
    const float baseSpread = trainingBaseSpread(activeWeapon);
    const float mouseSensitivity = std::clamp(settings_.gameplay.mouseSensitivity, 0.05f, 3.0f);
    const float mouseVerticalSensitivity = std::clamp(settings_.gameplay.mouseVerticalSensitivity, 0.5f, 3.0f);
    const float maxPitchRadians = degreesToRadians(std::clamp(settings_.gameplay.maxLookPitchDegrees, 45.0f, 88.0f));
    const float mouseYawScale = 0.0026f * mouseSensitivity;
    const float mousePitchScale = 0.0026f * mouseSensitivity * mouseVerticalSensitivity;

    singlePlayerCameraYawRadians_ += static_cast<float>(input.mouseDeltaX) * mouseYawScale;
    singlePlayerCameraPitchRadians_ = std::clamp(
        singlePlayerCameraPitchRadians_ - static_cast<float>(input.mouseDeltaY) * mousePitchScale,
        -maxPitchRadians,
        maxPitchRadians);

    if (input.turnLeftHeld) {
        singlePlayerCameraYawRadians_ -= turnSpeed * deltaSeconds;
    }
    if (input.turnRightHeld) {
        singlePlayerCameraYawRadians_ += turnSpeed * deltaSeconds;
    }
    fireCooldownSeconds_ = std::max(0.0f, fireCooldownSeconds_ - deltaSeconds);
    hitFlashSeconds_ = std::max(0.0f, hitFlashSeconds_ - deltaSeconds);
    muzzleFlashSeconds_ = std::max(0.0f, muzzleFlashSeconds_ - deltaSeconds);
    flashOverlaySeconds_ = std::max(0.0f, flashOverlaySeconds_ - deltaSeconds);
    smokeOverlaySeconds_ = std::max(0.0f, smokeOverlaySeconds_ - deltaSeconds);
    aimYawOffsetRadians_ = approachZero(aimYawOffsetRadians_,
        degreesToRadians(activeWeapon.recoilYaw) * handling.recoilReturnSpeed * deltaSeconds);
    viewKickAmount_ = std::max(0.0f, viewKickAmount_ - handling.viewKickScale * 3.1f * deltaSeconds);
    crosshairSpreadDegrees_ = std::max(baseSpread, crosshairSpreadDegrees_ - handling.spreadRecovery * deltaSeconds);

    const float forwardX = std::cos(singlePlayerCameraYawRadians_);
    const float forwardZ = std::sin(singlePlayerCameraYawRadians_);
    const float rightX = -forwardZ;
    const float rightZ = forwardX;

    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (input.moveForwardHeld) {
        moveX += forwardX;
        moveZ += forwardZ;
    }
    if (input.moveBackwardHeld) {
        moveX -= forwardX;
        moveZ -= forwardZ;
    }
    if (input.strafeRightHeld) {
        moveX += rightX;
        moveZ += rightZ;
    }
    if (input.strafeLeftHeld) {
        moveX -= rightX;
        moveZ -= rightZ;
    }

    singlePlayerVerticalVelocity_ -= kSinglePlayerGravity * deltaSeconds;
    singlePlayerJumpOffset_ = std::max(0.0f, singlePlayerJumpOffset_ + singlePlayerVerticalVelocity_ * deltaSeconds);
    if (singlePlayerJumpOffset_ <= 0.0f && singlePlayerVerticalVelocity_ < 0.0f) {
        singlePlayerVerticalVelocity_ = 0.0f;
    }

    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length > 0.0001f) {
        moveX /= length;
        moveZ /= length;

        crosshairSpreadDegrees_ = std::min(handling.maxSpread, crosshairSpreadDegrees_ + handling.moveSpreadGain * deltaSeconds);

        util::Vec3 next = singlePlayerCameraPosition_;
        next.x += moveX * moveSpeed * deltaSeconds;
        next.z += moveZ * moveSpeed * deltaSeconds;

        util::Vec3 testX = singlePlayerCameraPosition_;
        testX.x = next.x;
        if (!collidesWithWorld(testX)) {
            singlePlayerCameraPosition_.x = testX.x;
        }

        util::Vec3 testZ = singlePlayerCameraPosition_;
        testZ.z = next.z;
        if (!collidesWithWorld(testZ)) {
            singlePlayerCameraPosition_.z = testZ.z;
        }
    }

    if (singlePlayerJumpOffset_ > 0.0f) {
        crosshairSpreadDegrees_ = std::min(handling.maxSpread, crosshairSpreadDegrees_ + handling.moveSpreadGain * 1.25f * deltaSeconds);
    }

    const float groundHeight = gameplay::sampleFloorHeight(activeMap_, singlePlayerCameraPosition_.x, singlePlayerCameraPosition_.z);
    singlePlayerCameraPosition_.y = groundHeight + kSinglePlayerEyeHeight + singlePlayerJumpOffset_;
}

bool Application::collidesWithWorld(const util::Vec3& position) const {
    const float radius = 0.20f;
    const auto sampleSolid = [this](const float x, const float z) {
        if (x < 0.0f || z < 0.0f || x >= static_cast<float>(activeMap_.width) || z >= static_cast<float>(activeMap_.depth)) {
            return true;
        }

        const int cellX = static_cast<int>(std::floor(x));
        const int cellZ = static_cast<int>(std::floor(z));

        for (const auto& block : activeMap_.blocks) {
            if (block.solid && block.cell.x == cellX && block.cell.z == cellZ && block.cell.y >= 1) {
                return true;
            }
        }

        for (const auto& prop : activeMap_.props) {
            if (pointHitsPropFootprint(prop, x, z)) {
                return true;
            }
        }

        return false;
    };

    return sampleSolid(position.x - radius, position.z - radius) ||
           sampleSolid(position.x + radius, position.z - radius) ||
           sampleSolid(position.x - radius, position.z + radius) ||
           sampleSolid(position.x + radius, position.z + radius);
}

const content::WeaponDefinition* Application::findWeaponDefinition(const std::string& id) const {
    for (const auto& weapon : contentDatabase_.weapons()) {
        if (weapon.id == id) {
            return &weapon;
        }
    }
    return nullptr;
}

void Application::switchToNextTrainingWeapon() {
    if (trainingWeaponIds_.empty()) {
        return;
    }

    activeTrainingWeaponIndex_ = (activeTrainingWeaponIndex_ + 1) % trainingWeaponIds_.size();
    if (const auto* weapon = findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_])) {
        activeWeaponLabel_ = weapon->displayName;
        ammoInMagazine_ = weapon->magazineSize;
        reserveAmmo_ = weapon->reserveAmmo;
        fireCooldownSeconds_ = 0.0f;
        hitFlashSeconds_ = 0.0f;
        muzzleFlashSeconds_ = 0.0f;
        aimYawOffsetRadians_ = 0.0f;
        viewKickAmount_ = 0.0f;
        crosshairSpreadDegrees_ = trainingBaseSpread(*weapon);
        spdlog::info("[SinglePlayer] Switched weapon to {}.", weapon->displayName);
    }
}

void Application::selectTrainingEquipmentSlot(const TrainingEquipmentSlot slot) {
    if (activeTrainingSlot_ == slot) {
        return;
    }
    activeTrainingSlot_ = slot;
    needsRedraw_ = true;
}

void Application::cycleTrainingThrowable() {
    selectedThrowableIndex_ = (selectedThrowableIndex_ + 1) % 3;
    activeTrainingSlot_ = TrainingEquipmentSlot::Throwable;
    needsRedraw_ = true;
    spdlog::info("[SinglePlayer] 当前投掷物: {}.", throwableLabel(selectedThrowableIndex_));
}

void Application::cycleTrainingOptic() {
    switch (activeOptic_) {
        case content::OpticType::IronSight:
            activeOptic_ = content::OpticType::RedDot;
            break;
        case content::OpticType::RedDot:
            activeOptic_ = content::OpticType::X2;
            break;
        case content::OpticType::X2:
            activeOptic_ = content::OpticType::X4;
            break;
        case content::OpticType::X4:
            activeOptic_ = content::OpticType::X8;
            break;
        case content::OpticType::X8:
            activeOptic_ = content::OpticType::RedDot;
            break;
    }
    needsRedraw_ = true;
    spdlog::info("[SinglePlayer] 当前准镜: {}.", opticLabel(activeOptic_));
}

void Application::reloadTrainingWeapon() {
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    if (weapon == nullptr || reserveAmmo_ <= 0 || ammoInMagazine_ >= weapon->magazineSize) {
        return;
    }

    const int missing = weapon->magazineSize - ammoInMagazine_;
    const int moved = std::min(missing, reserveAmmo_);
    ammoInMagazine_ += moved;
    reserveAmmo_ -= moved;
    needsRedraw_ = true;
}

void Application::fireTrainingWeapon() {
    const auto* weapon = trainingWeaponIds_.empty() ? nullptr : findWeaponDefinition(trainingWeaponIds_[activeTrainingWeaponIndex_]);
    if (weapon == nullptr || fireCooldownSeconds_ > 0.0f) {
        return;
    }
    const WeaponHandlingProfile handling = makeHandlingProfile(*weapon);

    if (ammoInMagazine_ <= 0) {
        if (!settings_.gameplay.autoReload) {
            spdlog::info("[SinglePlayer] 弹匣已空，按 R 换弹。");
            return;
        }
        reloadTrainingWeapon();
        return;
    }

    --ammoInMagazine_;
    fireCooldownSeconds_ = 60.0f / std::max(1.0f, weapon->fireRateRpm);
    muzzleFlashSeconds_ = handling.muzzleFlashDuration;
    needsRedraw_ = true;

    const bool moving = lastInput_.moveForwardHeld || lastInput_.moveBackwardHeld ||
                        lastInput_.strafeLeftHeld || lastInput_.strafeRightHeld;
    const bool airborne = singlePlayerJumpOffset_ > 0.02f;
    const float baseSpread = trainingBaseSpread(*weapon);
    const float movementPenalty = moving ? weapon->hipSpread * 0.28f : 0.0f;
    const float airbornePenalty = airborne ? 1.1f : 0.0f;
    const float shotSpreadDegrees = std::min(handling.maxSpread,
        std::max(baseSpread, crosshairSpreadDegrees_) + movementPenalty + airbornePenalty);
    const float shotYaw = singlePlayerCameraYawRadians_ + aimYawOffsetRadians_ +
        degreesToRadians(randomSigned(recoilRandom_) * shotSpreadDegrees * 0.42f);
    const float shotPitch = singlePlayerCameraPitchRadians_ +
        degreesToRadians(randomSigned(recoilRandom_) * shotSpreadDegrees * 0.12f);
    const float forwardX = std::cos(shotYaw);
    const float forwardZ = std::sin(shotYaw);
    util::Vec3 origin = singlePlayerCameraPosition_;

    float bestDistance = std::numeric_limits<float>::max();
    gameplay::PlayerState* bestTarget = nullptr;
    for (auto& player : simulation_.players()) {
        if (!player.botControlled) {
            continue;
        }

        util::Vec3 toTarget{
            player.position.x - origin.x,
            0.0f,
            player.position.z - origin.z,
        };
        const float forwardDistance = toTarget.x * forwardX + toTarget.z * forwardZ;
        if (forwardDistance < 0.35f || forwardDistance > handling.effectiveRange) {
            continue;
        }

        const float lateralDistance = std::abs(toTarget.x * -forwardZ + toTarget.z * forwardX);
        if (lateralDistance > handling.targetRadius) {
            continue;
        }
        const float shotHeightAtTarget = origin.y + std::tan(shotPitch) * forwardDistance;
        const float verticalTolerance = 0.85f + handling.targetRadius * 0.25f;
        if (std::abs(player.position.y - shotHeightAtTarget) > verticalTolerance) {
            continue;
        }
        if (lineOfSightBlocked(origin, player.position)) {
            continue;
        }
        if (forwardDistance < bestDistance) {
            bestDistance = forwardDistance;
            bestTarget = &player;
        }
    }

    crosshairSpreadDegrees_ = std::min(handling.maxSpread,
        std::max(baseSpread, crosshairSpreadDegrees_) + handling.shotSpreadKick + movementPenalty * 0.55f + airbornePenalty);
    aimYawOffsetRadians_ += degreesToRadians(weapon->recoilYaw * handling.yawKickScale * randomSigned(recoilRandom_));
    const float maxYawKick = degreesToRadians(std::max(0.4f, weapon->recoilYaw * 2.8f));
    aimYawOffsetRadians_ = std::clamp(aimYawOffsetRadians_, -maxYawKick, maxYawKick);
    viewKickAmount_ = std::min(1.75f, viewKickAmount_ + weapon->recoilPitch * 0.085f * handling.viewKickScale);

    if (bestTarget == nullptr) {
        return;
    }

    bestTarget->health -= weapon->damage;
    hitFlashSeconds_ = 0.14f;
    crosshairSpreadDegrees_ = std::max(baseSpread, crosshairSpreadDegrees_ - handling.shotSpreadKick * 0.18f);
    if (bestTarget->health <= 0.0f) {
        ++eliminations_;
        bestTarget->health = 100.0f;
        bestTarget->position = {18.5f, 1.0f, 18.5f};
        bestTarget->velocity = {-0.15f, 0.0f, 0.0f};
        spdlog::info("[SinglePlayer] Target eliminated. Total eliminations: {}", eliminations_);
    }
}

void Application::useTrainingThrowable() {
    auto consumeThrowable = [](int& count) {
        if (count <= 0) {
            return false;
        }
        --count;
        return true;
    };

    const float forwardX = std::cos(singlePlayerCameraYawRadians_);
    const float forwardZ = std::sin(singlePlayerCameraYawRadians_);
    const util::Vec3 blastCenter{
        singlePlayerCameraPosition_.x + forwardX * 3.2f,
        singlePlayerCameraPosition_.y,
        singlePlayerCameraPosition_.z + forwardZ * 3.2f,
    };

    switch (selectedThrowableIndex_ % 3) {
        case 0: {
            if (!consumeThrowable(fragCount_)) {
                return;
            }
            int hitCount = 0;
            for (auto& player : simulation_.players()) {
                if (!player.botControlled) {
                    continue;
                }
                const float dx = player.position.x - blastCenter.x;
                const float dz = player.position.z - blastCenter.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                if (distance > 4.2f || lineOfSightBlocked(blastCenter, player.position)) {
                    continue;
                }
                player.health -= std::max(24.0f, 120.0f - distance * 22.0f);
                ++hitCount;
                if (player.health <= 0.0f) {
                    ++eliminations_;
                    player.health = 100.0f;
                    player.position = {18.5f, 1.0f, 18.5f};
                    player.velocity = {-0.15f, 0.0f, 0.0f};
                }
            }
            muzzleFlashSeconds_ = 0.10f;
            hitFlashSeconds_ = hitCount > 0 ? 0.14f : 0.0f;
            spdlog::info("[SinglePlayer] 投掷破片手雷，命中目标数: {}.", hitCount);
            break;
        }
        case 1:
            if (!consumeThrowable(flashCount_)) {
                return;
            }
            flashOverlaySeconds_ = 1.1f;
            spdlog::info("[SinglePlayer] 投掷闪光弹。");
            break;
        default:
            if (!consumeThrowable(smokeCount_)) {
                return;
            }
            smokeOverlaySeconds_ = 4.5f;
            spdlog::info("[SinglePlayer] 投掷烟雾弹。");
            break;
    }

    needsRedraw_ = true;
}

void Application::useTrainingMelee() {
    const float forwardX = std::cos(singlePlayerCameraYawRadians_);
    const float forwardZ = std::sin(singlePlayerCameraYawRadians_);
    gameplay::PlayerState* bestTarget = nullptr;
    float bestDistance = 1.65f;

    for (auto& player : simulation_.players()) {
        if (!player.botControlled) {
            continue;
        }
        const float dx = player.position.x - singlePlayerCameraPosition_.x;
        const float dz = player.position.z - singlePlayerCameraPosition_.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > bestDistance) {
            continue;
        }
        const float facing = (dx * forwardX + dz * forwardZ) / std::max(0.001f, distance);
        if (facing < 0.55f || lineOfSightBlocked(singlePlayerCameraPosition_, player.position)) {
            continue;
        }
        bestDistance = distance;
        bestTarget = &player;
    }

    muzzleFlashSeconds_ = 0.0f;
    if (bestTarget == nullptr) {
        needsRedraw_ = true;
        return;
    }

    bestTarget->health -= 55.0f;
    hitFlashSeconds_ = 0.10f;
    if (bestTarget->health <= 0.0f) {
        ++eliminations_;
        bestTarget->health = 100.0f;
        bestTarget->position = {18.5f, 1.0f, 18.5f};
        bestTarget->velocity = {-0.15f, 0.0f, 0.0f};
    }
    needsRedraw_ = true;
}

void Application::handleMapEditorInput(const platform::InputSnapshot& input) {
    if (input.primaryClickPressed) {
        if (selectMapEditorCellFromMouse(input.mouseX, input.mouseY)) {
            applyMapEditorTool();
        }
        return;
    }

    if (input.upPressed) {
        moveMapEditorCursor(0, -1);
    }
    if (input.downPressed) {
        moveMapEditorCursor(0, 1);
    }
    if (input.leftPressed) {
        moveMapEditorCursor(-1, 0);
    }
    if (input.rightPressed) {
        moveMapEditorCursor(1, 0);
    }

    if (input.selectPrimaryPressed) {
        mapEditorTool_ = MapEditorTool::Wall;
        mapEditorStatus_ = "已切换工具: 墙体";
        needsRedraw_ = true;
    }
    if (input.selectSecondaryPressed) {
        mapEditorTool_ = MapEditorTool::Crate;
        mapEditorStatus_ = "已切换工具: 箱体";
        needsRedraw_ = true;
    }
    if (input.selectMeleePressed) {
        mapEditorTool_ = MapEditorTool::AttackerSpawn;
        mapEditorStatus_ = "已切换工具: 进攻出生点";
        needsRedraw_ = true;
    }
    if (input.selectThrowablePressed) {
        mapEditorTool_ = MapEditorTool::DefenderSpawn;
        mapEditorStatus_ = "已切换工具: 防守出生点";
        needsRedraw_ = true;
    }
    if (input.selectToolFivePressed) {
        mapEditorTool_ = MapEditorTool::Eraser;
        mapEditorStatus_ = "已切换工具: 擦除";
        needsRedraw_ = true;
    }

    if (input.cycleThrowablePressed) {
        mapEditorTool_ = static_cast<MapEditorTool>((static_cast<int>(mapEditorTool_) + 1) % 5);
        mapEditorStatus_ = std::string("已切换工具: ") + mapEditorToolLabel();
        needsRedraw_ = true;
    }

    if (input.editorDeletePressed) {
        eraseMapEditorCell();
        return;
    }
    if (input.editorPreviousMapPressed) {
        cycleEditorMap(-1);
        return;
    }
    if (input.editorNextMapPressed) {
        cycleEditorMap(1);
        return;
    }
    if (input.editorNewMapPressed) {
        createNewEditorMap();
        return;
    }
    if (input.editorSavePressed || input.cycleOpticPressed) {
        saveActiveMapArtifacts("编辑器保存");
        mapEditorStatus_ = "地图与预览图已保存";
        needsRedraw_ = true;
        return;
    }
    if (input.confirmPressed) {
        applyMapEditorTool();
    }
}

void Application::handleMapBrowserInput(const platform::InputSnapshot& input) {
    if (input.primaryClickPressed) {
        const std::size_t hitIndex = hitTestMapBrowserItem(input.mouseX, input.mouseY);
        if (hitIndex != std::numeric_limits<std::size_t>::max()) {
            loadEditorMapByIndex(hitIndex);
            activateSelectedMapBrowserItem();
        }
        return;
    }

    if (input.upPressed || input.editorPreviousMapPressed) {
        cycleEditorMap(-1);
    }
    if (input.downPressed || input.editorNextMapPressed) {
        cycleEditorMap(1);
    }
    if (input.editorNewMapPressed) {
        createNewEditorMap();
        if (mapBrowserTargetFlow_ == AppFlow::MapEditor) {
            currentFlow_ = AppFlow::MapEditor;
            syncInputMode();
            refreshWindowTitle();
            logCurrentFlow();
        }
        needsRedraw_ = true;
        return;
    }
    if (input.confirmPressed) {
        activateSelectedMapBrowserItem();
    }
}

void Application::handleMultiplayerLobbyInput(const platform::InputSnapshot& input) {
    if (input.upPressed) {
        navigateMultiplayerLobby(-1);
    }
    if (input.downPressed) {
        navigateMultiplayerLobby(1);
    }
    if (input.leftPressed) {
        adjustSelectedMultiplayerSetting(-1);
    }
    if (input.rightPressed) {
        adjustSelectedMultiplayerSetting(1);
    }
    if (input.confirmPressed) {
        activateSelectedMultiplayerSetting();
    }
}

void Application::moveMapEditorCursor(const int dx, const int dz) {
    const int nextX = std::clamp(mapEditorCursorX_ + dx, 0, std::max(0, activeMap_.width - 1));
    const int nextZ = std::clamp(mapEditorCursorZ_ + dz, 0, std::max(0, activeMap_.depth - 1));
    if (nextX == mapEditorCursorX_ && nextZ == mapEditorCursorZ_) {
        return;
    }
    mapEditorCursorX_ = nextX;
    mapEditorCursorZ_ = nextZ;
    mapEditorStatus_ = "已移动编辑光标";
    needsRedraw_ = true;
}

bool Application::selectMapEditorCellFromMouse(const int mouseX, const int mouseY) {
    if (window_ == nullptr) {
        return false;
    }

    const int width = std::max(1, window_->clientWidth());
    const int height = std::max(1, window_->clientHeight());
    constexpr float kCanvasX0 = 0.30f;
    constexpr float kCanvasY0 = 0.10f;
    constexpr float kCanvasX1 = 0.92f;
    constexpr float kCanvasY1 = 0.86f;
    const float normalizedX = static_cast<float>(mouseX) / static_cast<float>(width);
    const float normalizedY = static_cast<float>(mouseY) / static_cast<float>(height);
    if (normalizedX < kCanvasX0 || normalizedX > kCanvasX1 || normalizedY < kCanvasY0 || normalizedY > kCanvasY1) {
        return false;
    }

    const float localX = (normalizedX - kCanvasX0) / (kCanvasX1 - kCanvasX0);
    const float localY = (normalizedY - kCanvasY0) / (kCanvasY1 - kCanvasY0);
    mapEditorCursorX_ = std::clamp(static_cast<int>(localX * static_cast<float>(activeMap_.width)), 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(static_cast<int>(localY * static_cast<float>(activeMap_.depth)), 0, std::max(0, activeMap_.depth - 1));
    mapEditorStatus_ = "已通过鼠标选中格子";
    needsRedraw_ = true;
    return true;
}

void Application::applyMapEditorTool() {
    const int cellX = mapEditorCursorX_;
    const int cellZ = mapEditorCursorZ_;

    switch (mapEditorTool_) {
        case MapEditorTool::Wall: {
            const bool hasCover = std::ranges::any_of(activeMap_.blocks, [cellX, cellZ](const gameplay::MapBlock& block) {
                return block.cell.x == cellX && block.cell.z == cellZ && block.solid && block.cell.y >= 1;
            });
            if (hasCover) {
                activeMap_.blocks.erase(std::remove_if(activeMap_.blocks.begin(), activeMap_.blocks.end(),
                    [cellX, cellZ](const gameplay::MapBlock& block) {
                        return block.cell.x == cellX && block.cell.z == cellZ && block.solid && block.cell.y >= 1;
                    }), activeMap_.blocks.end());
                mapEditorStatus_ = "已移除墙体/掩体";
            } else {
                activeMap_.blocks.push_back(gameplay::MapBlock{{cellX, 1, cellZ}, "wall_cover", true});
                mapEditorStatus_ = "已放置墙体/掩体";
            }
            break;
        }
        case MapEditorTool::Crate: {
            const auto crateModel = assetRoot_ / "source" / "polyhaven" / "models" / "wooden_crate_02" / "wooden_crate_02_1k.gltf";
            const auto crateMaterial = assetRoot_ / "generated" / "materials" / "polyhaven_wooden_crate_02.mat";
            activeMap_.props.erase(std::remove_if(activeMap_.props.begin(), activeMap_.props.end(),
                [cellX, cellZ](const gameplay::MapProp& prop) {
                    return positionInsideCell(prop.position, cellX, cellZ);
                }), activeMap_.props.end());
            activeMap_.props.push_back(gameplay::MapProp{"editor_crate", centerOfCell(cellX, cellZ, 0.0f), crateModel, crateMaterial});
            mapEditorStatus_ = "已放置箱体";
            break;
        }
        case MapEditorTool::AttackerSpawn: {
            activeMap_.spawns.erase(std::remove_if(activeMap_.spawns.begin(), activeMap_.spawns.end(),
                [](const gameplay::SpawnPoint& spawn) {
                    return spawn.team == gameplay::Team::Attackers;
                }), activeMap_.spawns.end());
            activeMap_.spawns.push_back(gameplay::SpawnPoint{gameplay::Team::Attackers, centerOfCell(cellX, cellZ, 1.0f)});
            mapEditorStatus_ = "已设置进攻出生点";
            break;
        }
        case MapEditorTool::DefenderSpawn: {
            activeMap_.spawns.erase(std::remove_if(activeMap_.spawns.begin(), activeMap_.spawns.end(),
                [](const gameplay::SpawnPoint& spawn) {
                    return spawn.team == gameplay::Team::Defenders;
                }), activeMap_.spawns.end());
            activeMap_.spawns.push_back(gameplay::SpawnPoint{gameplay::Team::Defenders, centerOfCell(cellX, cellZ, 1.0f)});
            mapEditorStatus_ = "已设置防守出生点";
            break;
        }
        case MapEditorTool::Eraser:
            eraseMapEditorCell();
            return;
    }

    needsRedraw_ = true;
}

void Application::eraseMapEditorCell() {
    const int cellX = mapEditorCursorX_;
    const int cellZ = mapEditorCursorZ_;
    const std::size_t beforeBlockCount = activeMap_.blocks.size();
    const std::size_t beforePropCount = activeMap_.props.size();
    const std::size_t beforeSpawnCount = activeMap_.spawns.size();

    activeMap_.blocks.erase(std::remove_if(activeMap_.blocks.begin(), activeMap_.blocks.end(),
        [cellX, cellZ](const gameplay::MapBlock& block) {
            return block.cell.x == cellX && block.cell.z == cellZ && block.cell.y >= 1;
        }), activeMap_.blocks.end());
    activeMap_.props.erase(std::remove_if(activeMap_.props.begin(), activeMap_.props.end(),
        [cellX, cellZ](const gameplay::MapProp& prop) {
            return positionInsideCell(prop.position, cellX, cellZ);
        }), activeMap_.props.end());
    activeMap_.spawns.erase(std::remove_if(activeMap_.spawns.begin(), activeMap_.spawns.end(),
        [cellX, cellZ](const gameplay::SpawnPoint& spawn) {
            return positionInsideCell(spawn.position, cellX, cellZ);
        }), activeMap_.spawns.end());

    const bool changed = activeMap_.blocks.size() != beforeBlockCount ||
        activeMap_.props.size() != beforePropCount ||
        activeMap_.spawns.size() != beforeSpawnCount;
    mapEditorStatus_ = changed ? "已清除当前格内容" : "当前格没有可清除内容";
    needsRedraw_ = true;
}

void Application::openMapBrowser(const AppFlow targetFlow) {
    mapBrowserTargetFlow_ = targetFlow;
    refreshMapCatalog();
    if (!mapCatalogPaths_.empty()) {
        loadEditorMapByIndex(activeMapCatalogIndex_);
    } else {
        mapEditorStatus_ = "没有可用地图";
    }
    currentFlow_ = AppFlow::MapBrowser;
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::activateSelectedMapBrowserItem() {
    refreshMapCatalog();
    if (!mapCatalogPaths_.empty()) {
        loadEditorMapByIndex(activeMapCatalogIndex_);
    }

    currentFlow_ = mapBrowserTargetFlow_;
    if (currentFlow_ == AppFlow::SinglePlayerLobby) {
        initializeSinglePlayerView();
        restartNetworkSession(network::SessionType::Offline, "切换到单机离线模式");
    } else if (currentFlow_ == AppFlow::MapEditor) {
        mapEditorStatus_ = std::string("正在编辑地图: ") + activeMapPath_.stem().string();
    } else if (currentFlow_ == AppFlow::MultiPlayerLobby) {
        selectedMultiplayerIndex_ = 0;
        multiplayerStatus_ = std::string("已选择地图: ") + activeMapPath_.stem().string();
    }
    syncInputMode();
    refreshWindowTitle();
    logCurrentFlow();
    needsRedraw_ = true;
}

void Application::navigateMultiplayerLobby(const int delta) {
    const int count = static_cast<int>(kMultiplayerEntryCount);
    const int current = static_cast<int>(selectedMultiplayerIndex_);
    selectedMultiplayerIndex_ = static_cast<std::size_t>((current + delta + count) % count);
    needsRedraw_ = true;
}

void Application::adjustSelectedMultiplayerSetting(const int delta) {
    switch (selectedMultiplayerIndex_) {
        case 0:
            multiplayerSessionType_ = multiplayerSessionType_ == network::SessionType::Host
                ? network::SessionType::Client
                : network::SessionType::Host;
            multiplayerStatus_ = std::string("联机模式切换为") + sessionTypeLabel(multiplayerSessionType_);
            needsRedraw_ = true;
            break;
        case 1: {
            const int nextPort = std::clamp(static_cast<int>(settings_.network.port) + delta * 5, 1025, 65530);
            settings_.network.port = static_cast<std::uint16_t>(nextPort);
            saveSettings(settings_, settingsPath_);
            multiplayerStatus_ = "端口已更新";
            needsRedraw_ = true;
            break;
        }
        case 2: {
            settings_.network.maxPlayers = std::clamp(settings_.network.maxPlayers + delta, 2, 32);
            saveSettings(settings_, settingsPath_);
            multiplayerStatus_ = "房间人数已更新";
            needsRedraw_ = true;
            break;
        }
        case 3:
            if (delta != 0) {
                activateSelectedMultiplayerSetting();
            }
            break;
        default:
            break;
    }
}

void Application::activateSelectedMultiplayerSetting() {
    if (selectedMultiplayerIndex_ == 3) {
        restartNetworkSession(multiplayerSessionType_, "联机房间已启动");
        multiplayerStatus_ = std::string("已启动") + sessionTypeLabel(multiplayerSessionType_) +
            "会话，地图 " + activeMap_.name;
        needsRedraw_ = true;
        return;
    }
    adjustSelectedMultiplayerSetting(1);
}

void Application::restartNetworkSession(const network::SessionType type, const char* reason) {
    networkSession_.stop();
    networkSession_ = network::NetworkSession({
        .type = type,
        .endpoint = {settings_.network.defaultServerHost, settings_.network.port},
    });
    const bool started = networkSession_.start();
    multiplayerSessionActive_ = type != network::SessionType::Offline && started;
    if (!started) {
        spdlog::error("[Network] {} 失败: {}:{}",
            reason,
            settings_.network.defaultServerHost,
            settings_.network.port);
        return;
    }
    spdlog::info("[Network] {} | type={} endpoint={}:{} maxPlayers={}",
        reason,
        sessionTypeLabel(type),
        settings_.network.defaultServerHost,
        settings_.network.port,
        settings_.network.maxPlayers);
}

void Application::refreshMapCatalog() {
    mapCatalogPaths_.clear();
    const auto mapsRoot = assetRoot_ / "maps";
    std::error_code error;
    if (std::filesystem::exists(mapsRoot, error) && !error) {
        for (const auto& entry : std::filesystem::directory_iterator(mapsRoot, error)) {
            if (error || !entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() == ".arena") {
                mapCatalogPaths_.push_back(entry.path());
            }
        }
    }

    std::sort(mapCatalogPaths_.begin(), mapCatalogPaths_.end());
    if (mapCatalogPaths_.empty()) {
        activeMapPath_ = mapsRoot / "depot_lab.arena";
        activeMapCatalogIndex_ = 0;
        return;
    }

    auto match = std::find(mapCatalogPaths_.begin(), mapCatalogPaths_.end(), activeMapPath_);
    if (match == mapCatalogPaths_.end()) {
        activeMapPath_ = mapCatalogPaths_.front();
        activeMapCatalogIndex_ = 0;
    } else {
        activeMapCatalogIndex_ = static_cast<std::size_t>(std::distance(mapCatalogPaths_.begin(), match));
    }
}

void Application::loadEditorMapByIndex(const std::size_t index) {
    refreshMapCatalog();
    if (mapCatalogPaths_.empty()) {
        return;
    }

    activeMapCatalogIndex_ = std::min(index, mapCatalogPaths_.size() - 1);
    activeMapPath_ = mapCatalogPaths_[activeMapCatalogIndex_];
    const gameplay::MapData loadedMap = gameplay::MapSerializer::load(activeMapPath_);
    if (!loadedMap.blocks.empty() || !loadedMap.props.empty() || !loadedMap.spawns.empty() || !loadedMap.name.empty()) {
        activeMap_ = loadedMap;
    }
    mapEditorCursorX_ = std::clamp(mapEditorCursorX_, 0, std::max(0, activeMap_.width - 1));
    mapEditorCursorZ_ = std::clamp(mapEditorCursorZ_, 0, std::max(0, activeMap_.depth - 1));
    mapEditorStatus_ = std::string("已加载地图: ") + activeMapPath_.stem().string();
    needsRedraw_ = true;
    spdlog::info("[MapEditor] Loaded map: {}", activeMapPath_.generic_string());
}

void Application::cycleEditorMap(const int delta) {
    refreshMapCatalog();
    if (mapCatalogPaths_.empty()) {
        return;
    }

    const int count = static_cast<int>(mapCatalogPaths_.size());
    const int current = static_cast<int>(activeMapCatalogIndex_);
    const std::size_t nextIndex = static_cast<std::size_t>((current + delta + count) % count);
    loadEditorMapByIndex(nextIndex);
}

gameplay::MapData Application::makeBlankEditorMap(const std::string& name) const {
    gameplay::MapEditor editor(gameplay::MapData{
        .name = name,
        .width = 24,
        .height = 8,
        .depth = 24,
        .blocks = {},
        .spawns = {},
        .props = {},
        .lights = {},
    });
    editor.paintFloor(0, "floor_concrete");
    editor.paintPerimeterWalls(3, "wall_concrete");
    editor.addSpawn(gameplay::Team::Attackers, {3.5f, 1.0f, 3.5f});
    editor.addSpawn(gameplay::Team::Defenders, {20.5f, 1.0f, 20.5f});
    return editor.map();
}

std::filesystem::path Application::nextCustomMapPath() const {
    const auto mapsRoot = assetRoot_ / "maps";
    for (int index = 1; index <= 999; ++index) {
        std::ostringstream name;
        name << "custom_map_" << std::setw(2) << std::setfill('0') << index << ".arena";
        const auto candidate = mapsRoot / name.str();
        if (!util::FileSystem::exists(candidate)) {
            return candidate;
        }
    }
    return mapsRoot / "custom_map_overflow.arena";
}

void Application::createNewEditorMap() {
    activeMapPath_ = nextCustomMapPath();
    activeMap_ = makeBlankEditorMap("Custom Arena " + activeMapPath_.stem().string());
    mapEditorCursorX_ = 3;
    mapEditorCursorZ_ = 3;
    saveActiveMapArtifacts("新建地图");
    refreshMapCatalog();
    mapEditorStatus_ = std::string("已新建地图: ") + activeMapPath_.stem().string();
    needsRedraw_ = true;
    spdlog::info("[MapEditor] Created new map slot: {}", activeMapPath_.generic_string());
}

void Application::saveActiveMapArtifacts(const char* reason) {
    if (activeMapPath_.empty()) {
        activeMapPath_ = assetRoot_ / "maps" / "depot_lab.arena";
    }
    gameplay::MapSerializer::save(activeMap_, activeMapPath_);
    const auto previewPath = assetRoot_ / "generated" / (activeMapPath_.stem().string() + "_preview.ppm");
    gameplay::MapEditor(activeMap_).exportTopDownPreview(previewPath);
    refreshMapCatalog();
    spdlog::info("[MapEditor] {} -> {}", reason, activeMapPath_.generic_string());
}

const char* Application::mapEditorToolLabel() const {
    switch (mapEditorTool_) {
        case MapEditorTool::Wall:
            return "墙体";
        case MapEditorTool::Crate:
            return "箱体";
        case MapEditorTool::AttackerSpawn:
            return "进攻出生点";
        case MapEditorTool::DefenderSpawn:
            return "防守出生点";
        case MapEditorTool::Eraser:
            return "擦除";
    }
    return "墙体";
}

void Application::persistSettings(const char* reason) {
    saveSettings(settings_, settingsPath_);
    needsRedraw_ = true;
    spdlog::info("[Settings] {} | 灵敏度={} 垂直倍率={} 俯仰上限={} 自动换弹={}",
        reason,
        settings_.gameplay.mouseSensitivity,
        settings_.gameplay.mouseVerticalSensitivity,
        settings_.gameplay.maxLookPitchDegrees,
        settingToggleLabel(settings_.gameplay.autoReload));
}

bool Application::lineOfSightBlocked(const util::Vec3& from, const util::Vec3& to) const {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.001f) {
        return false;
    }

    const int steps = std::max(1, static_cast<int>(distance * 12.0f));
    for (int step = 1; step < steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const float sampleX = from.x + dx * t;
        const float sampleZ = from.z + dz * t;
        if (collidesWithWorld({sampleX, 1.0f, sampleZ})) {
            return true;
        }
    }
    return false;
}

std::size_t Application::hitTestMainMenuItem(const int mouseX, const int mouseY) const {
    const int width = window_ != nullptr ? window_->clientWidth() : 0;
    const int height = window_ != nullptr ? window_->clientHeight() : 0;
    if (width <= 0 || height <= 0) {
        return selectedMenuIndex_;
    }

    const auto inRect = [mouseX, mouseY, width, height](float x0, float y0, float x1, float y1) {
        const int left = static_cast<int>(width * x0);
        const int top = static_cast<int>(height * y0);
        const int right = static_cast<int>(width * x1);
        const int bottom = static_cast<int>(height * y1);
        return mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom;
    };

    const auto& items = mainMenu_.items();
    for (std::size_t index = 0; index < items.size(); ++index) {
        const float top = 0.24f + static_cast<float>(index) * 0.11f;
        const float bottom = top + 0.085f;
        if (inRect(0.045f, top, 0.44f, bottom)) {
            return index;
        }
    }

    return selectedMenuIndex_;
}

std::size_t Application::hitTestMapBrowserItem(const int mouseX, const int mouseY) const {
    const int width = window_ != nullptr ? window_->clientWidth() : 0;
    const int height = window_ != nullptr ? window_->clientHeight() : 0;
    if (width <= 0 || height <= 0 || mapCatalogPaths_.empty()) {
        return std::numeric_limits<std::size_t>::max();
    }

    const auto inRect = [mouseX, mouseY, width, height](float x0, float y0, float x1, float y1) {
        const int left = static_cast<int>(width * x0);
        const int top = static_cast<int>(height * y0);
        const int right = static_cast<int>(width * x1);
        const int bottom = static_cast<int>(height * y1);
        return mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom;
    };

    for (std::size_t index = 0; index < mapCatalogPaths_.size(); ++index) {
        const float top = 0.24f + static_cast<float>(index) * 0.10f;
        const float bottom = top + 0.075f;
        if (inRect(0.08f, top, 0.62f, bottom)) {
            return index;
        }
    }

    return std::numeric_limits<std::size_t>::max();
}

}  // namespace mycsg::app
