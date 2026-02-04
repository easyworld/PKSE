#include "Globals.h"
#include "UI/TitleSelectionScreen.h"
#include "UI/Common.h"
#include "Utils/HelperUtilities.h"
#include "Utils/Logger.h"
#include "Enums/GameVersion.h"

using namespace Utils;
using namespace Enums;

namespace UI {
    // UI Layout constants
    constexpr int LEFT_PANEL_X = 0;
    constexpr int LEFT_PANEL_Y = 70;
    constexpr int LEFT_TITLE_SELECTION_PANEL_WIDTH = 500;
    constexpr int LEFT_TITLE_SELECTION_PANEL_HEIGHT = 500;

    TitleSelectionScreen::TitleSelectionScreen(AccountUid userUid)
        : userUid(userUid), selectedIndex(0), titleSelected(false), goBack(false) {
        loadTitles();
    }

    void TitleSelectionScreen::loadTitles() {
        titles.clear();

        NsApplicationRecord records[100];
        s32 recordCount = 0;

        Result rc = nsListApplicationRecord(records, 100, 0, &recordCount);
        if (R_FAILED(rc)) {
            logErrorToFile("Failed to list application records");
            return;
        }

        for (s32 i = 0; i < recordCount; i++) {
            u64 titleId = records[i].application_id;

            // Filter to show only Pokemon titles using isPokemonTitle function
            GameVersion gameVersion = getGameVersion(titleId);
            if (gameVersion != GameVersion::Invalid) {
                // Check if the selected user has save data for this title
                Result mountResult = fsdevMountSaveData("save_check", titleId, userUid);
                if (R_SUCCEEDED(mountResult)) {
                    // User has save data for this title
                    fsdevUnmountDevice("save_check");

                    TitleInfo info;
                    info.titleId = titleId;
                    // Apply sanitization and formatting to handle special characters
                    info.name = "宝可梦 " + getGameVersionName(gameVersion);
                    titles.push_back(info);
                }
                // If mount failed, user doesn't have save data - skip this title
            }
        }

        if (titles.empty()) {
            TitleInfo info;
            info.titleId = 0;
            info.name = "未找到该用户的宝可梦存档";
            titles.push_back(info);
        }
    }

    void TitleSelectionScreen::update(const PadState& pad) {
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Up) {
            selectedIndex = (selectedIndex - 1 + titles.size()) % titles.size();
        }
        if (kDown & HidNpadButton_Down) {
            selectedIndex = (selectedIndex + 1) % titles.size();
        }
        if (kDown & HidNpadButton_A) {
            if (!titles.empty() && titles[selectedIndex].titleId != 0) {
                selectedTitleId = titles[selectedIndex].titleId;
                selectedTitleName = titles[selectedIndex].name;
                titleSelected = true;
            }
        }
        if (kDown & HidNpadButton_B || kDown & HidNpadButton_Plus) {
            goBack = true;
        }
    }

    void TitleSelectionScreen::draw(PKSEFramebuffer& fb) {
        fb.clear(Colors::Background);

        // Draw title bar
        std::string versionText = "PKSE - 宝可梦存档编辑器 v" + VERSION_STRING;
        fb.drawFilledRect(0, 0, fb.getWidth(), 60, Colors::Panel);
        fb.drawText(20, 20,versionText.c_str(), Colors::Text);
        fb.drawRect(0, 0, fb.getWidth(), 60, Colors::Border);

        // Draw title selection panel
        fb.drawFilledRect(LEFT_PANEL_X, LEFT_PANEL_Y, LEFT_TITLE_SELECTION_PANEL_WIDTH, LEFT_TITLE_SELECTION_PANEL_HEIGHT, Colors::Panel);
        fb.drawRect(LEFT_PANEL_X, LEFT_PANEL_Y, LEFT_TITLE_SELECTION_PANEL_WIDTH, LEFT_TITLE_SELECTION_PANEL_HEIGHT, Colors::Border);

        // Draw panel title
        fb.drawText(LEFT_PANEL_X + 20, LEFT_PANEL_Y + 20, "选择宝可梦游戏", Colors::Text);
        fb.drawFilledRect(LEFT_PANEL_X + 20, LEFT_PANEL_Y + 45, LEFT_TITLE_SELECTION_PANEL_WIDTH - 40, 2, Colors::Border);

        // Draw title list
        drawTitleList(fb);

        // Draw instructions
        fb.drawText(LEFT_PANEL_X + 20, LEFT_PANEL_Y + LEFT_TITLE_SELECTION_PANEL_HEIGHT + 20, "按 A 选择  |  按 B 返回", Colors::TextDim);
    }

    void TitleSelectionScreen::drawTitleList(PKSEFramebuffer& fb) {
        int itemHeight = 50;
        int startY = LEFT_PANEL_Y + 60;

        for (size_t i = 0; i < titles.size(); i++) {
            int itemY = startY + (i * itemHeight);

            if ((int)i == selectedIndex) {
                fb.drawFilledRect(LEFT_PANEL_X + 10, itemY, 480, itemHeight - 5, Colors::Selected);
            }

            std::string displayText = "> " + titles[i].name;
            fb.drawText(LEFT_PANEL_X + 30, itemY + 15, displayText, Colors::Text);
        }
    }
}
