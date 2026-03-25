#include "platform/Window.h"

#include <memory>

namespace mycsg::platform {

namespace {

class NullWindow final : public IWindow {
public:
    bool create(const WindowConfig&) override { return true; }
    void pollEvents() override {}
    bool shouldClose() const override { return frameCount_++ > 5; }
    void* nativeHandle() const override { return nullptr; }
    void* nativeWindowObject() const override { return nullptr; }
    InputSnapshot consumeInput() override { return {}; }
    void setTitle(const std::string&) override {}
    void requestClose() override { closed_ = true; }
    int clientWidth() const override { return 1600; }
    int clientHeight() const override { return 900; }
    void setRelativeMouseMode(bool) override {}
    void setNativeEventObserver(NativeEventObserver) override {}

private:
    mutable int frameCount_ = 0;
    bool closed_ = false;
};

}  // namespace

std::unique_ptr<IWindow> createWindow() {
    return std::make_unique<NullWindow>();
}

}  // namespace mycsg::platform
