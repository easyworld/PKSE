#include <cstring>
#include <cstdio>

#include "Globals.h"
#include "UI/SaveSelectScreen.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"
#include "UI/SystemIcons.h"
#include "UI/TouchInput.h"
#include "Enums/GameVersion.h"
#include "Utils/Logger.h"

using namespace Utils;
using namespace Enums;

namespace UI {
    // Layout (1280x720).
    constexpr int HEADER_Y   = 82;    // top of the user header card
    constexpr int HEADER_H   = 104;
    constexpr int AVATAR     = 84;
    constexpr int CHIP       = 56;    // small per-user switcher avatar
    constexpr int TILE_W     = 184;
    constexpr int TILE_H     = 200;
    constexpr int ICON       = 140;
    constexpr int GAP        = 24;
    constexpr int MAX_COLS   = 5;
    constexpr int GRID_Y     = 208;

    SaveSelectScreen::SaveSelectScreen() {
        loadUsers();
    }

    void SaveSelectScreen::loadUsers() {
        users.clear();

        AccountUid userIds[ACC_USER_LIST_SIZE];
        s32 userCount = 0;
        Result rc = accountListAllUsers(userIds, ACC_USER_LIST_SIZE, &userCount);
        if (R_FAILED(rc)) {
            logErrorToFile("SaveSelect: failed to list users");
            userCount = 0;
        }

        for (s32 i = 0; i < userCount; i++) {
            UserEntry entry;
            entry.uid = userIds[i];

            AccountProfile profile;
            AccountProfileBase base;
            if (R_SUCCEEDED(accountGetProfile(&profile, userIds[i]))) {
                if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &base))) {
                    entry.name = std::string(base.nickname);
                } else {
                    entry.name = "未知用户";
                }
                accountProfileClose(&profile);
            } else {
                entry.name = "未知用户";
            }

            loadTitlesForUser(entry);
            users.push_back(std::move(entry));
        }

        if (users.empty()) {
            UserEntry def;
            def.name = "未找到用户";
            memset(&def.uid, 0, sizeof(AccountUid));
            users.push_back(std::move(def));
        }
    }

    void SaveSelectScreen::loadTitlesForUser(UserEntry& user) {
        NsApplicationRecord records[100];
        s32 recordCount = 0;
        Result rc = nsListApplicationRecord(records, 100, 0, &recordCount);
        if (R_FAILED(rc)) {
            logErrorToFile("SaveSelect: failed to list application records");
            return;
        }

        for (s32 i = 0; i < recordCount; i++) {
            u64 titleId = records[i].application_id;
            GameVersion gv = getGameVersion(titleId);
            if (gv == GameVersion::Invalid) continue;   // not a PKSE-supported title

            // Only include titles the selected user actually has a save for. Use a UNIQUE mount name
            // per title: re-mounting one shared name in a tight loop can fail if the previous unmount
            // hasn't fully released the devoptab slot, which silently drops every other title (and would
            // consistently hide one of each Sword/Shield, LGP/LGE pair depending on enumeration order).
            char dev[16];
            snprintf(dev, sizeof(dev), "svchk%d", (int)i);
            Result mountResult = fsdevMountSaveData(dev, titleId, user.uid);
            if (R_FAILED(mountResult)) {
                char m[176];
                snprintf(m, sizeof(m), "SaveSelect: %s (%016llX) recognized but has no mountable save for this user (rc=0x%08X)",
                         getGameVersionName(gv).c_str(), (unsigned long long)titleId, (unsigned)mountResult);
                logInfoToFile(m);
                continue;
            }
            fsdevUnmountDevice(dev);
            {
                char m[128];
                snprintf(m, sizeof(m), "SaveSelect: %s (%016llX) -> listed", getGameVersionName(gv).c_str(), (unsigned long long)titleId);
                logInfoToFile(m);
            }

            TitleEntry t;
            t.titleId = titleId;
            t.label = getGameVersionName(gv);       // short: "盾", "Legends: Z-A", ...
            t.name  = "宝可梦 " + t.label;         // full name (backup dir + downstream compat)
            user.titles.push_back(std::move(t));
        }
    }

    const SaveSelectScreen::UserEntry* SaveSelectScreen::currentUser() const {
        if (users.empty()) return nullptr;
        return &users[userIndex];
    }

    int SaveSelectScreen::titleColumns() const {
        const UserEntry* u = currentUser();
        int count = u ? (int)u->titles.size() : 0;
        if (count <= 0) return 1;
        return count < MAX_COLS ? count : MAX_COLS;
    }

    void SaveSelectScreen::setUser(int idx) {
        if (users.empty()) return;
        userIndex = (idx % (int)users.size() + (int)users.size()) % (int)users.size();
        titleIndex = 0;
    }

    void SaveSelectScreen::selectCurrentTitle() {
        const UserEntry* u = currentUser();
        if (!u || titleIndex < 0 || titleIndex >= (int)u->titles.size()) return;
        selectedUserUid  = u->uid;
        selectedTitleId  = u->titles[titleIndex].titleId;
        selectedTitleName = u->titles[titleIndex].name;
        titleSelected = true;
    }

    void SaveSelectScreen::update(const PadState& pad, const TouchInput& touch) {
        // A tap on a nav-bar badge becomes that button's press, so every handler below is
        // reached identically whether the user pressed the button or tapped its on-screen badge.
        u64 kDown = padGetButtonsDown(&pad) | navTouchButton(touch);

        // Touch (tap targets were captured last draw()).
        if (touch.justPressed()) {
            const int tx = touch.x(), ty = touch.y();
            bool handled = false;
            for (const auto& r : userRects) {
                if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h) {
                    setUser(r.idx); handled = true; break;
                }
            }
            if (!handled) {
                for (const auto& r : titleRects) {
                    if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h) {
                        titleIndex = r.idx; kDown |= HidNpadButton_A; break;
                    }
                }
            }
        }

        // Switch users with the shoulder buttons (only when there's more than one).
        if (users.size() > 1) {
            if (kDown & HidNpadButton_L) setUser(userIndex - 1);
            if (kDown & HidNpadButton_R) setUser(userIndex + 1);
        }

        // Title grid navigation.
        const UserEntry* u = currentUser();
        int count = u ? (int)u->titles.size() : 0;
        if (count > 0) {
            int cols = titleColumns();
            if (kDown & HidNpadButton_Left)  titleIndex = (titleIndex - 1 + count) % count;
            if (kDown & HidNpadButton_Right) titleIndex = (titleIndex + 1) % count;
            if ((kDown & HidNpadButton_Down) && titleIndex + cols < count) titleIndex += cols;
            if ((kDown & HidNpadButton_Up)   && titleIndex - cols >= 0)    titleIndex -= cols;
            if (kDown & HidNpadButton_A) selectCurrentTitle();
        }

        if (kDown & HidNpadButton_Plus) exitRequested = true;
    }

    void SaveSelectScreen::draw(PKSEFramebuffer& fb) {
        titleRects.clear();
        userRects.clear();

        fb.clear(Colors::Background);
        drawTitleBar(fb, "宝可梦存档编辑器   v" + VERSION_STRING);

        const UserEntry* u = currentUser();

        // ---- User header card ----
        fb.drawCard(24, HEADER_Y, fb.getWidth() - 48, HEADER_H);

        const int avX = 44, avY = HEADER_Y + (HEADER_H - AVATAR) / 2;
        if (u) {
            const IconImage& av = SystemIcons::userIcon(u->uid);
            if (av.valid())
                fb.drawImageScaled(avX, avY, av.width, av.height, AVATAR, AVATAR, av.data, 4);
            else
                fb.drawFilledRoundedRect(avX, avY, AVATAR, AVATAR, 10, Colors::Selected);
            fb.drawRoundedRect(avX, avY, AVATAR, AVATAR, 10, Colors::Accent, 2);
        }

        const int nameX = avX + AVATAR + 20;
        if (u) {
            int nlh = fb.lineHeight(TextStyle::Title);
            fb.drawText(nameX, avY + 6, u->name, Colors::Text, TextStyle::Title);
            int cnt = (int)u->titles.size();
            std::string sub = std::to_string(cnt) + (cnt == 1 ? " 个宝可梦存档" : " 个宝可梦存档");
            fb.drawText(nameX, avY + 6 + nlh + 4, sub, Colors::TextDim, TextStyle::Caption);
        }

        // Per-user switcher chips (only when more than one account).
        if (users.size() > 1) {
            int n = (int)users.size();
            int totalW = n * CHIP + (n - 1) * 12;
            int startX = fb.getWidth() - 40 - totalW;
            int chipY = HEADER_Y + (HEADER_H - CHIP) / 2;
            for (int i = 0; i < n; i++) {
                int cx = startX + i * (CHIP + 12);
                const IconImage& av = SystemIcons::userIcon(users[i].uid);
                if (av.valid())
                    fb.drawImageScaled(cx, chipY, av.width, av.height, CHIP, CHIP, av.data, 4);
                else
                    fb.drawFilledRoundedRect(cx, chipY, CHIP, CHIP, 8, Colors::Selected);
                if (i == userIndex) fb.drawRoundedRect(cx, chipY, CHIP, CHIP, 8, Colors::Primary, 3);
                else                fb.drawRoundedRect(cx, chipY, CHIP, CHIP, 8, Colors::Border, 1);
                userRects.push_back({cx, chipY, CHIP, CHIP, i});
            }
        }

        // ---- Title grid ----
        int count = u ? (int)u->titles.size() : 0;
        if (count == 0) {
            const char* msg = "未找到该用户的宝可梦存档";
            int mw, mh; fb.measureText(msg, mw, mh, TextStyle::Body);
            fb.drawText((fb.getWidth() - mw) / 2, GRID_Y + 120, msg, Colors::TextDim, TextStyle::Body);
        } else {
            int cols = titleColumns();
            int gridW = cols * TILE_W + (cols - 1) * GAP;
            int startX = (fb.getWidth() - gridW) / 2;
            if (startX < 40) startX = 40;

            for (int i = 0; i < count; i++) {
                int col = i % cols, row = i / cols;
                int tileX = startX + col * (TILE_W + GAP);
                int tileY = GRID_Y + row * (TILE_H + GAP);
                bool sel = (i == titleIndex);

                fb.drawSoftShadow(tileX, tileY, TILE_W, TILE_H, 16);
                fb.drawFilledRoundedRect(tileX, tileY, TILE_W, TILE_H, 16, sel ? Colors::PanelAlt : Colors::Panel);
                if (sel) fb.drawRoundedRect(tileX, tileY, TILE_W, TILE_H, 16, Colors::Primary, 3);
                else     fb.drawRoundedRect(tileX, tileY, TILE_W, TILE_H, 16, Colors::Border, 1);

                int iconX = tileX + (TILE_W - ICON) / 2;
                int iconY = tileY + 16;
                const IconImage& ic = SystemIcons::titleIcon(u->titles[i].titleId);
                if (ic.valid())
                    fb.drawImageScaled(iconX, iconY, ic.width, ic.height, ICON, ICON, ic.data, 4);
                else
                    fb.drawFilledRoundedRect(iconX, iconY, ICON, ICON, 10, Colors::PanelAlt);

                // Short label centered under the icon.
                const std::string& label = u->titles[i].label;
                int lw, lh; fb.measureText(label, lw, lh, TextStyle::Caption);
                fb.drawText(tileX + (TILE_W - lw) / 2, iconY + ICON + 8, label,
                            sel ? Colors::Text : Colors::TextDim, TextStyle::Caption);

                titleRects.push_back({tileX, tileY, TILE_W, TILE_H, i});
            }
        }

        // ---- Footer ----
        if (users.size() > 1)
            drawNavBar(fb, "A：选择  |  L/R：切换用户  |  +：退出");
        else
            drawNavBar(fb, "A：选择  |  +：退出");
    }
}
