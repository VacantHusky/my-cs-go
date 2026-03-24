#include "app/Application.h"
#include "util/Log.h"

int main() {
    mycsg::util::initializeLogging();
    mycsg::app::Application app;
    return app.run();
}
