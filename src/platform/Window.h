#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace mycsg::platform {

struct WindowConfig {
    int width = 1600;
    int height = 900;
    std::string title = "my-cs-go";
};

struct InputSnapshot {
    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;
    bool confirmPressed = false;
    bool backPressed = false;
    bool firePressed = false;
    bool jumpPressed = false;
    bool reloadPressed = false;
    bool switchWeaponPressed = false;
    bool selectPrimaryPressed = false;
    bool selectSecondaryPressed = false;
    bool selectMeleePressed = false;
    bool selectThrowablePressed = false;
    bool selectToolFivePressed = false;
    bool cycleThrowablePressed = false;
    bool cycleOpticPressed = false;
    bool ctrlHeld = false;
    bool editorSavePressed = false;
    bool editorDeletePressed = false;
    bool editorPreviousMapPressed = false;
    bool editorNextMapPressed = false;
    bool editorNewMapPressed = false;
    bool editorUndoPressed = false;
    bool editorToggleProjectionPressed = false;
    bool moveForwardHeld = false;
    bool moveBackwardHeld = false;
    bool moveUpHeld = false;
    bool moveDownHeld = false;
    bool strafeLeftHeld = false;
    bool strafeRightHeld = false;
    bool turnLeftHeld = false;
    bool turnRightHeld = false;
    bool primaryClickPressed = false;
    bool secondaryClickHeld = false;
    int mouseX = 0;
    int mouseY = 0;
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
};

struct NativeEventObserver {
    void (*callback)(const void* event, void* userData) = nullptr;
    void* userData = nullptr;
};

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual bool create(const WindowConfig& config) = 0;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void* nativeHandle() const = 0;
    virtual void* nativeWindowObject() const = 0;
    virtual InputSnapshot consumeInput() = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void requestClose() = 0;
    virtual int clientWidth() const = 0;
    virtual int clientHeight() const = 0;
    virtual void setRelativeMouseMode(bool enabled) = 0;
    virtual void setNativeEventObserver(NativeEventObserver observer) = 0;
};

std::unique_ptr<IWindow> createWindow();

}  // namespace mycsg::platform
