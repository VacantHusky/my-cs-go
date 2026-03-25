#include "platform/Window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <memory>

namespace mycsg::platform {

namespace {

constexpr Uint64 kUiRepeatInitialDelayMs = 180;
constexpr Uint64 kUiRepeatIntervalMs = 45;

struct RepeatState {
    bool armed = false;
    Uint64 nextTriggerTick = 0;
};

class Win32Window final : public IWindow {
public:
    ~Win32Window() override {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        if (sdlInitialized_) {
            SDL_Quit();
            sdlInitialized_ = false;
        }
    }

    bool create(const WindowConfig& config) override {
        if (!sdlInitialized_) {
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                return false;
            }
            sdlInitialized_ = true;
        }

        window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (window_ == nullptr) {
            return false;
        }

        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        return true;
    }

    void pollEvents() override {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (eventObserver_.callback != nullptr) {
                eventObserver_.callback(&event, eventObserver_.userData);
            }
            switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    shouldClose_ = true;
                    break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    hasFocus_ = true;
                    break;
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    hasFocus_ = false;
                    accumulatedMouseDeltaX_ = 0;
                    accumulatedMouseDeltaY_ = 0;
                    accumulatedMouseWheelDelta_ = 0;
                    ctrlHeld_ = false;
                    moveDownHeld_ = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    onKeyDown(event.key.scancode, event.key.repeat);
                    break;
                case SDL_EVENT_KEY_UP:
                    onKeyUp(event.key.scancode);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    input_.mouseX = static_cast<int>(event.motion.x);
                    input_.mouseY = static_cast<int>(event.motion.y);
                    accumulatedMouseDeltaX_ += static_cast<int>(event.motion.xrel);
                    accumulatedMouseDeltaY_ += static_cast<int>(event.motion.yrel);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input_.mouseX = static_cast<int>(event.button.x);
                        input_.mouseY = static_cast<int>(event.button.y);
                        input_.primaryClickPressed = true;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        input_.mouseX = static_cast<int>(event.button.x);
                        input_.mouseY = static_cast<int>(event.button.y);
                        secondaryClickHeld_ = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        secondaryClickHeld_ = false;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    accumulatedMouseWheelDelta_ += static_cast<int>(event.wheel.y);
                    break;
                default:
                    break;
            }
        }

        updateUiRepeat();
    }

    bool shouldClose() const override {
        return shouldClose_;
    }

    void* nativeHandle() const override {
        if (window_ == nullptr) {
            return nullptr;
        }
        const SDL_PropertiesID props = SDL_GetWindowProperties(window_);
        return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    }

    void* nativeWindowObject() const override {
        return window_;
    }

    InputSnapshot consumeInput() override {
        InputSnapshot snapshot = input_;
        snapshot.ctrlHeld = ctrlHeld_;
        snapshot.moveForwardHeld = moveForwardHeld_;
        snapshot.moveBackwardHeld = moveBackwardHeld_;
        snapshot.moveUpHeld = moveUpHeld_;
        snapshot.moveDownHeld = moveDownHeld_;
        snapshot.strafeLeftHeld = strafeLeftHeld_;
        snapshot.strafeRightHeld = strafeRightHeld_;
        snapshot.turnLeftHeld = turnLeftHeld_;
        snapshot.turnRightHeld = turnRightHeld_;
        snapshot.secondaryClickHeld = secondaryClickHeld_;
        snapshot.mouseDeltaX = accumulatedMouseDeltaX_;
        snapshot.mouseDeltaY = accumulatedMouseDeltaY_;
        snapshot.mouseWheelDelta = accumulatedMouseWheelDelta_;

        input_.upPressed = false;
        input_.downPressed = false;
        input_.leftPressed = false;
        input_.rightPressed = false;
        input_.confirmPressed = false;
        input_.backPressed = false;
        input_.firePressed = false;
        input_.jumpPressed = false;
        input_.reloadPressed = false;
        input_.switchWeaponPressed = false;
        input_.selectPrimaryPressed = false;
        input_.selectSecondaryPressed = false;
        input_.selectMeleePressed = false;
        input_.selectThrowablePressed = false;
        input_.selectToolFivePressed = false;
        input_.cycleThrowablePressed = false;
        input_.cycleOpticPressed = false;
        input_.editorSavePressed = false;
        input_.editorDeletePressed = false;
        input_.editorPreviousMapPressed = false;
        input_.editorNextMapPressed = false;
        input_.editorNewMapPressed = false;
        input_.editorUndoPressed = false;
        input_.editorToggleProjectionPressed = false;
        input_.primaryClickPressed = false;
        accumulatedMouseDeltaX_ = 0;
        accumulatedMouseDeltaY_ = 0;
        accumulatedMouseWheelDelta_ = 0;
        return snapshot;
    }

    void setTitle(const std::string& title) override {
        if (window_ != nullptr) {
            SDL_SetWindowTitle(window_, title.c_str());
        }
    }

    void requestClose() override {
        shouldClose_ = true;
    }

    int clientWidth() const override {
        int width = 0;
        int height = 0;
        if (window_ != nullptr) {
            SDL_GetWindowSizeInPixels(window_, &width, &height);
        }
        return width;
    }

    int clientHeight() const override {
        int width = 0;
        int height = 0;
        if (window_ != nullptr) {
            SDL_GetWindowSizeInPixels(window_, &width, &height);
        }
        return height;
    }

    void setRelativeMouseMode(const bool enabled) override {
        relativeMouseMode_ = enabled;
        if (window_ != nullptr) {
            SDL_SetWindowRelativeMouseMode(window_, enabled);
        }
    }

    void setNativeEventObserver(const NativeEventObserver observer) override {
        eventObserver_ = observer;
    }

private:
    static void triggerRepeat(const Uint64 now,
                              const bool held,
                              RepeatState& state,
                              bool& pressedFlag) {
        if (!held) {
            state.armed = false;
            state.nextTriggerTick = 0;
            return;
        }
        if (!state.armed) {
            state.armed = true;
            state.nextTriggerTick = now + kUiRepeatInitialDelayMs;
            return;
        }
        if (now >= state.nextTriggerTick) {
            pressedFlag = true;
            state.nextTriggerTick = now + kUiRepeatIntervalMs;
        }
    }

    void updateUiRepeat() {
        const Uint64 now = SDL_GetTicks();
        triggerRepeat(now, moveForwardHeld_, upRepeatState_, input_.upPressed);
        triggerRepeat(now, moveBackwardHeld_, downRepeatState_, input_.downPressed);
        triggerRepeat(now, strafeLeftHeld_ || turnLeftHeld_, leftRepeatState_, input_.leftPressed);
        triggerRepeat(now, strafeRightHeld_ || turnRightHeld_, rightRepeatState_, input_.rightPressed);
        triggerRepeat(now, previousMapHeld_, previousMapRepeatState_, input_.editorPreviousMapPressed);
        triggerRepeat(now, nextMapHeld_, nextMapRepeatState_, input_.editorNextMapPressed);
    }

    void onKeyDown(const SDL_Scancode key, const bool repeat) {
        switch (key) {
            case SDL_SCANCODE_UP:
                if (repeat) {
                    return;
                }
                input_.upPressed = true;
                moveForwardHeld_ = true;
                break;
            case SDL_SCANCODE_DOWN:
                if (repeat) {
                    return;
                }
                input_.downPressed = true;
                moveBackwardHeld_ = true;
                break;
            case SDL_SCANCODE_LEFT:
                if (repeat) {
                    return;
                }
                input_.leftPressed = true;
                turnLeftHeld_ = true;
                break;
            case SDL_SCANCODE_RIGHT:
                if (repeat) {
                    return;
                }
                input_.rightPressed = true;
                turnRightHeld_ = true;
                break;
            case SDL_SCANCODE_W:
                if (repeat) {
                    return;
                }
                input_.upPressed = true;
                moveForwardHeld_ = true;
                break;
            case SDL_SCANCODE_S:
                if (repeat) {
                    return;
                }
                input_.downPressed = true;
                moveBackwardHeld_ = true;
                break;
            case SDL_SCANCODE_A:
                if (repeat) {
                    return;
                }
                input_.leftPressed = true;
                strafeLeftHeld_ = true;
                break;
            case SDL_SCANCODE_D:
                if (repeat) {
                    return;
                }
                input_.rightPressed = true;
                strafeRightHeld_ = true;
                break;
            case SDL_SCANCODE_Q:
                if (repeat) {
                    return;
                }
                input_.editorPreviousMapPressed = true;
                previousMapHeld_ = true;
                turnLeftHeld_ = true;
                break;
            case SDL_SCANCODE_E:
                if (repeat) {
                    return;
                }
                input_.editorNextMapPressed = true;
                nextMapHeld_ = true;
                turnRightHeld_ = true;
                break;
            case SDL_SCANCODE_RETURN:
                if (repeat) {
                    return;
                }
                input_.confirmPressed = true;
                input_.firePressed = true;
                break;
            case SDL_SCANCODE_SPACE:
                if (repeat) {
                    return;
                }
                input_.confirmPressed = true;
                input_.jumpPressed = true;
                moveUpHeld_ = true;
                break;
            case SDL_SCANCODE_LCTRL:
            case SDL_SCANCODE_RCTRL:
                if (repeat) {
                    return;
                }
                ctrlHeld_ = true;
                moveDownHeld_ = true;
                break;
            case SDL_SCANCODE_Z:
                if (repeat) {
                    return;
                }
                if (ctrlHeld_) {
                    input_.editorUndoPressed = true;
                }
                break;
            case SDL_SCANCODE_O:
                if (repeat) {
                    return;
                }
                input_.editorToggleProjectionPressed = true;
                break;
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
                if (repeat) {
                    return;
                }
                input_.backPressed = true;
                break;
            case SDL_SCANCODE_R:
                if (repeat) {
                    return;
                }
                input_.reloadPressed = true;
                break;
            case SDL_SCANCODE_TAB:
                if (repeat) {
                    return;
                }
                input_.switchWeaponPressed = true;
                break;
            case SDL_SCANCODE_1:
                if (repeat) {
                    return;
                }
                input_.selectPrimaryPressed = true;
                break;
            case SDL_SCANCODE_2:
                if (repeat) {
                    return;
                }
                input_.selectSecondaryPressed = true;
                break;
            case SDL_SCANCODE_3:
                if (repeat) {
                    return;
                }
                input_.selectMeleePressed = true;
                break;
            case SDL_SCANCODE_4:
                if (repeat) {
                    return;
                }
                input_.selectThrowablePressed = true;
                break;
            case SDL_SCANCODE_5:
                if (repeat) {
                    return;
                }
                input_.selectToolFivePressed = true;
                break;
            case SDL_SCANCODE_G:
                if (repeat) {
                    return;
                }
                input_.cycleThrowablePressed = true;
                break;
            case SDL_SCANCODE_V:
                if (repeat) {
                    return;
                }
                input_.cycleOpticPressed = true;
                break;
            case SDL_SCANCODE_F5:
                if (repeat) {
                    return;
                }
                input_.editorSavePressed = true;
                break;
            case SDL_SCANCODE_F6:
                if (repeat) {
                    return;
                }
                input_.editorNewMapPressed = true;
                break;
            case SDL_SCANCODE_DELETE:
                if (repeat) {
                    return;
                }
                input_.editorDeletePressed = true;
                break;
            default:
                break;
        }
    }

    void onKeyUp(const SDL_Scancode key) {
        switch (key) {
            case SDL_SCANCODE_UP:
            case SDL_SCANCODE_W:
                moveForwardHeld_ = false;
                break;
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_S:
                moveBackwardHeld_ = false;
                break;
            case SDL_SCANCODE_SPACE:
                moveUpHeld_ = false;
                break;
            case SDL_SCANCODE_LCTRL:
            case SDL_SCANCODE_RCTRL:
                ctrlHeld_ = false;
                moveDownHeld_ = false;
                break;
            case SDL_SCANCODE_A:
                strafeLeftHeld_ = false;
                break;
            case SDL_SCANCODE_D:
                strafeRightHeld_ = false;
                break;
            case SDL_SCANCODE_LEFT:
                turnLeftHeld_ = false;
                break;
            case SDL_SCANCODE_Q:
                previousMapHeld_ = false;
                turnLeftHeld_ = false;
                break;
            case SDL_SCANCODE_RIGHT:
                turnRightHeld_ = false;
                break;
            case SDL_SCANCODE_E:
                nextMapHeld_ = false;
                turnRightHeld_ = false;
                break;
            default:
                break;
        }
    }

    SDL_Window* window_ = nullptr;
    bool sdlInitialized_ = false;
    bool shouldClose_ = false;
    bool hasFocus_ = true;
    bool relativeMouseMode_ = false;
    InputSnapshot input_{};
    bool moveForwardHeld_ = false;
    bool moveBackwardHeld_ = false;
    bool moveUpHeld_ = false;
    bool moveDownHeld_ = false;
    bool ctrlHeld_ = false;
    bool strafeLeftHeld_ = false;
    bool strafeRightHeld_ = false;
    bool turnLeftHeld_ = false;
    bool turnRightHeld_ = false;
    bool previousMapHeld_ = false;
    bool nextMapHeld_ = false;
    bool secondaryClickHeld_ = false;
    int accumulatedMouseDeltaX_ = 0;
    int accumulatedMouseDeltaY_ = 0;
    int accumulatedMouseWheelDelta_ = 0;
    NativeEventObserver eventObserver_{};
    RepeatState upRepeatState_{};
    RepeatState downRepeatState_{};
    RepeatState leftRepeatState_{};
    RepeatState rightRepeatState_{};
    RepeatState previousMapRepeatState_{};
    RepeatState nextMapRepeatState_{};
};

}  // namespace

std::unique_ptr<IWindow> createWindow() {
    return std::make_unique<Win32Window>();
}

}  // namespace mycsg::platform
