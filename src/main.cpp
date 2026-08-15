#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <bits/stdc++.h>

#include "Globals.h"
#include "Save/GetSaveFileContents.h"
#include "UI/SpriteManager.h"
#include "UI/SystemIcons.h"
#include "UI/UI.h"
#include "Utils/Logger.h"
#include "Utils/HelperUtilities.h"
#include "Utils/FileUtilities.h"
#include "Utils/Settings.h"

int main()
{
    // Settings FIRST, before the first log line. "Enable Debug Logging" gates every sink, so
    // loading it up here is what makes the toggle cover startup as well -- read any later and the
    // dozen lines below would follow the compiled-in default instead of the user's choice, which
    // means either losing exactly the init diagnostics a bug report needs or writing a file the
    // user switched off. Safe this early: loadSettings only fopen()s sdmc:/PKSE/settings.cfg
    // (libnx mounts sdmc before main), applyTheme just swaps colour globals, and nothing in it
    // logs. It needs no service, no ROMFS and no SDL, none of which exist yet.
    Utils::loadSettings();

    logInfoToFile("Initializing PKSE...");

    Utils::cleanupOldLogs();

    // Initialize the ns service
    Result nsServiceInitializeResult = nsInitialize();
    if (R_FAILED(nsServiceInitializeResult)) {
        logErrorToFile("Failed to initialize ns service");
        return -1;
    }

    // Initialize account service
    Result accountServiceInitializeResult = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(accountServiceInitializeResult)) {
        logErrorToFile("Failed to initialize account service");
        nsExit();
        return -1;
    }

    // Initialize ROMFS for accessing bundled sprites
    Utils::logInfoToFile("Initializing ROMFS...");
    Result romfsInitResult = romfsInit();
    bool romfsInitialized = false;
    if (R_FAILED(romfsInitResult)) {
        logErrorToFile("Failed to initialize ROMFS - sprites will not be available");
        // Don't exit, app can still run without sprites
    } else {
        Utils::logInfoToFile("ROMFS initialized successfully");
        romfsInitialized = true;
    }

    // Initialize SDL for the window, GL context and input only -- rendering is NanoVG on GL.
    // SDL_image and SDL_ttf are deliberately NOT initialised: PNGs are decoded by stb_image in
    // SpriteManager and text is drawn by NanoVG's own font atlas, so neither library is used.
    Utils::logInfoToFile("Initializing SDL...");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logErrorToFile("SDL_Init(VIDEO) failed");
        logErrorToFile(SDL_GetError());
    }

    // Initialize sprite manager for Pokemon images
    Utils::logInfoToFile("Initializing Sprite Manager...");
    UI::SpriteManager::init();

    Utils::logInfoToFile("Testing sprite loading...");
    UI::Sprite* testSprite = UI::SpriteManager::getSprite(25, false); // Pikachu
    if (testSprite && testSprite->data) {
        logInfoToFile(("SUCCESS: Test sprite loaded! (" +
            std::to_string(testSprite->width) + "x" +
            std::to_string(testSprite->height) + ")").c_str());
    } else {
        Utils::logInfoToFile("WARNING: Test sprite failed to load - sprites may not be available");
    }

    Utils::logInfoToFile("Starting UI Manager...");

    {
        UI::UIManager uiManager;
        uiManager.run();
    }  // UIManager (and its SDL-backed framebuffer) destroyed here, before SDL_Quit

    // Cleanup
    Utils::logInfoToFile("Cleaning up Sprite Manager...");
    UI::SpriteManager::cleanup();
    UI::SystemIcons::cleanup();

    SDL_Quit();

    if (romfsInitialized) {
        Utils::logInfoToFile("Cleaning up ROMFS...");
        romfsExit();
    }

    accountExit();
    nsExit();

    return 0;
}
