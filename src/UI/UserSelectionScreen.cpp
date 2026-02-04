#include <cstring>

#include "Globals.h"
#include "UI/UserSelectionScreen.h"
#include "UI/Common.h"
#include "Utils/Logger.h"

using namespace Utils;

namespace UI {
    // UI Layout constants
    constexpr int LEFT_PANEL_X = 0;
    constexpr int LEFT_PANEL_Y = 70;

    UserSelectionScreen::UserSelectionScreen()
        : selectedIndex(0), userSelected(false), exitRequested(false) {
        loadUsers();
    }

    void UserSelectionScreen::loadUsers() {
        users.clear();

        AccountUid userIds[ACC_USER_LIST_SIZE];
        s32 userCount = 0;

        Result rc = accountListAllUsers(userIds, ACC_USER_LIST_SIZE, &userCount);
        if (R_FAILED(rc)) {
            logErrorToFile("Failed to list users");
            return;
        }

        for (s32 i = 0; i < userCount; i++) {
            UserInfo info;
            info.uid = userIds[i];

            AccountProfile profile;
            AccountProfileBase base;

            rc = accountGetProfile(&profile, userIds[i]);
            if (R_SUCCEEDED(rc)) {
                rc = accountProfileGet(&profile, NULL, &base);
                if (R_SUCCEEDED(rc)) {
                    info.name = std::string(base.nickname);
                    info.isValid = true;
                } else {
                    info.name = "未知用户";
                    info.isValid = false;
                }
                accountProfileClose(&profile);
            } else {
                info.name = "未知用户";
                info.isValid = false;
            }

            users.push_back(info);
        }

        if (users.empty()) {
            UserInfo defaultUser;
            defaultUser.name = "默认用户";
            defaultUser.isValid = false;
            memset(&defaultUser.uid, 0, sizeof(AccountUid));
            users.push_back(defaultUser);
        }
    }

    void UserSelectionScreen::update(const PadState& pad) {
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Up) {
            selectedIndex = (selectedIndex - 1 + users.size()) % users.size();
        }
        if (kDown & HidNpadButton_Down) {
            selectedIndex = (selectedIndex + 1) % users.size();
        }
        if (kDown & HidNpadButton_A) {
            if (!users.empty()) {
                selectedUserUid = users[selectedIndex].uid;
                userSelected = true;
            }
        }
        if (kDown & HidNpadButton_Plus) {
            exitRequested = true;
        }
    }

    void UserSelectionScreen::draw(PKSEFramebuffer& fb) {
        fb.clear(Colors::Background);

        // Draw title bar
        std::string versionText = "PKSE - 宝可梦存档编辑器 v" + VERSION_STRING;
        fb.drawFilledRect(0, 0, fb.getWidth(), 60, Colors::Panel);
        fb.drawText(20, 20, versionText, Colors::Text);
        fb.drawRect(0, 0, fb.getWidth(), 60, Colors::Border);

        // Draw user selection panel
        int panelWidth = 500;
        int panelHeight = 500;

        fb.drawFilledRect(LEFT_PANEL_X, LEFT_PANEL_Y, panelWidth, panelHeight, Colors::Panel);
        fb.drawRect(LEFT_PANEL_X, LEFT_PANEL_Y, panelWidth, panelHeight, Colors::Border);

        // Draw panel title
        fb.drawText(LEFT_PANEL_X + 20, LEFT_PANEL_Y + 20, "选择用户档案", Colors::Text);
        fb.drawFilledRect(LEFT_PANEL_X + 20, LEFT_PANEL_Y + 45, panelWidth - 40, 2, Colors::Border);

        // Draw user list
        drawUserList(fb);

        // Draw instructions
        fb.drawText(LEFT_PANEL_X + 20, LEFT_PANEL_Y + panelHeight + 20, "按 A 选择  |  按 + 退出", Colors::TextDim);
    }

    void UserSelectionScreen::drawUserList(PKSEFramebuffer& fb) {
        int itemHeight = 50;
        int startY = LEFT_PANEL_Y + 60;

        for (size_t i = 0; i < users.size(); i++) {
            int itemY = startY + (i * itemHeight);

            if ((int)i == selectedIndex) {
                fb.drawFilledRect(LEFT_PANEL_X + 10, itemY, 380, itemHeight - 5, Colors::Selected);
            }

            std::string displayText = "> " + users[i].name;
            fb.drawText(LEFT_PANEL_X + 30, itemY + 15, displayText, Colors::Text);
        }
    }
}
