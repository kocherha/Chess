#pragma once

#include "axmol.h"

class AppDelegate : private ax::Application
{
public:
    AppDelegate();
    ~AppDelegate() override;

    // The command line is read before the window exists; see GameScene::Launch
    // for what it can ask for.
    static void parseArguments(int argc, char** argv);

    // The main loop: start up, run until the window is closed, shut down.
    // Used instead of Application::run(), which never stops asking the system
    // whether anything has happened; this one waits to be told. See the .cpp.
    int runUntilClosed();

    void initGfxContextAttrs() override;
    bool applicationDidFinishLaunching() override;
    void applicationDidEnterBackground() override;
    void applicationWillEnterForeground() override;
};
