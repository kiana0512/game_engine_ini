#include "core/App.h"

int main(int, char**)
{
    App app;
    if (!app.init(1280, 720, "K-Engine")) return -1;
    app.run();
    app.shutdown();
    return 0;
}
