#include "platform/Window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <memory>

namespace mycsg::platform {

namespace {

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
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (!event.key.repeat) {
                        onKeyDown(event.key.scancode);
                    }
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
                    }
                    break;
                default:
                    break;
            }
        }
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
        snapshot.moveForwardHeld = moveForwardHeld_;
        snapshot.moveBackwardHeld = moveBackwardHeld_;
        snapshot.strafeLeftHeld = strafeLeftHeld_;
        snapshot.strafeRightHeld = strafeRightHeld_;
        snapshot.turnLeftHeld = turnLeftHeld_;
        snapshot.turnRightHeld = turnRightHeld_;
        snapshot.mouseDeltaX = accumulatedMouseDeltaX_;
        snapshot.mouseDeltaY = accumulatedMouseDeltaY_;

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
        input_.primaryClickPressed = false;
        accumulatedMouseDeltaX_ = 0;
        accumulatedMouseDeltaY_ = 0;
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

private:
    void onKeyDown(const SDL_Scancode key) {
        switch (key) {
            case SDL_SCANCODE_UP:
                input_.upPressed = true;
                moveForwardHeld_ = true;
                break;
            case SDL_SCANCODE_DOWN:
                input_.downPressed = true;
                moveBackwardHeld_ = true;
                break;
            case SDL_SCANCODE_LEFT:
                input_.leftPressed = true;
                turnLeftHeld_ = true;
                break;
            case SDL_SCANCODE_RIGHT:
                input_.rightPressed = true;
                turnRightHeld_ = true;
                break;
            case SDL_SCANCODE_W:
                input_.upPressed = true;
                moveForwardHeld_ = true;
                break;
            case SDL_SCANCODE_S:
                input_.downPressed = true;
                moveBackwardHeld_ = true;
                break;
            case SDL_SCANCODE_A:
                input_.leftPressed = true;
                strafeLeftHeld_ = true;
                break;
            case SDL_SCANCODE_D:
                input_.rightPressed = true;
                strafeRightHeld_ = true;
                break;
            case SDL_SCANCODE_Q:
                input_.editorPreviousMapPressed = true;
                turnLeftHeld_ = true;
                break;
            case SDL_SCANCODE_E:
                input_.editorNextMapPressed = true;
                turnRightHeld_ = true;
                break;
            case SDL_SCANCODE_RETURN:
                input_.confirmPressed = true;
                input_.firePressed = true;
                break;
            case SDL_SCANCODE_SPACE:
                input_.confirmPressed = true;
                input_.jumpPressed = true;
                break;
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
                input_.backPressed = true;
                break;
            case SDL_SCANCODE_R:
                input_.reloadPressed = true;
                break;
            case SDL_SCANCODE_TAB:
                input_.switchWeaponPressed = true;
                break;
            case SDL_SCANCODE_1:
                input_.selectPrimaryPressed = true;
                break;
            case SDL_SCANCODE_2:
                input_.selectSecondaryPressed = true;
                break;
            case SDL_SCANCODE_3:
                input_.selectMeleePressed = true;
                break;
            case SDL_SCANCODE_4:
                input_.selectThrowablePressed = true;
                break;
            case SDL_SCANCODE_5:
                input_.selectToolFivePressed = true;
                break;
            case SDL_SCANCODE_G:
                input_.cycleThrowablePressed = true;
                break;
            case SDL_SCANCODE_V:
                input_.cycleOpticPressed = true;
                break;
            case SDL_SCANCODE_F5:
                input_.editorSavePressed = true;
                break;
            case SDL_SCANCODE_F6:
                input_.editorNewMapPressed = true;
                break;
            case SDL_SCANCODE_DELETE:
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
            case SDL_SCANCODE_A:
                strafeLeftHeld_ = false;
                break;
            case SDL_SCANCODE_D:
                strafeRightHeld_ = false;
                break;
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_Q:
                turnLeftHeld_ = false;
                break;
            case SDL_SCANCODE_RIGHT:
            case SDL_SCANCODE_E:
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
    bool strafeLeftHeld_ = false;
    bool strafeRightHeld_ = false;
    bool turnLeftHeld_ = false;
    bool turnRightHeld_ = false;
    int accumulatedMouseDeltaX_ = 0;
    int accumulatedMouseDeltaY_ = 0;
};

}  // namespace

std::unique_ptr<IWindow> createWindow() {
    return std::make_unique<Win32Window>();
}

}  // namespace mycsg::platform
