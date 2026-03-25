#include "app/Application.h"
#include "util/Log.h"

#include <csignal>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void crashSignalHandler(const int signalValue) {
    std::fprintf(stderr, "[Crash] C signal %d captured.\n", signalValue);
    std::fflush(stderr);
    std::_Exit(128 + signalValue);
}

#ifdef _WIN32
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers) {
    const unsigned long code = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
        ? exceptionPointers->ExceptionRecord->ExceptionCode
        : 0ul;
    std::fprintf(stderr, "[Crash] Unhandled SEH exception: 0x%08lX\n", code);
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void installCrashHandlers() {
    std::signal(SIGABRT, crashSignalHandler);
    std::signal(SIGFPE, crashSignalHandler);
    std::signal(SIGILL, crashSignalHandler);
    std::signal(SIGSEGV, crashSignalHandler);
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
}

}  // namespace

int main() {
    installCrashHandlers();
    mycsg::util::initializeLogging();
    mycsg::app::Application app(mycsg::app::ApplicationLaunchMode::Editor);
    return app.run();
}
