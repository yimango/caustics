#include "app.h"

int main(int argc, char** argv) {
    App app;
    if (!app.parseArgs(argc, argv)) {
        return 1;
    }
    if (!app.init()) {
        return -1;
    }
    return app.run();
}
