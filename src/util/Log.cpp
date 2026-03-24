#include "util/Log.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace mycsg::util {

void initializeLogging() {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    auto logger = spdlog::stdout_color_mt("my-cs-go");
    logger->set_pattern("%v");
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(logger));
    initialized = true;
}

}  // namespace mycsg::util
