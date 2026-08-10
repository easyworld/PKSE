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
    // Rows of tiles that fit between the grid's top and the nav bar: (720 - 56 - 208) = 456px of
    // band against a 224px row pitch. Anything past this scrolls rather than being drawn off the
    // bottom edge, which is how titles eleven and twelve used to vanish.
    constexpr int VISIBLE_ROWS = 2;

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

    /**
     * List the Pokemon saves this user has, by enumerating SAVE DATA -- not installed titles.
     *
     * The distinction is the whole point. This used to walk `nsListApplicationRecord` and, for each
     * Pokemon title found, probe whether a save could be mounted. That asks "which games are
     * installed, and do they have saves?" when the only question a save editor cares about is
     * "which saves exist?" -- and the two differ in a case that is not rare at all:
     *
     *   A game played from a CARTRIDGE has no application record when the cart is out. Its save
     *   lives on internal storage and is perfectly editable, but the title vanishes from the picker
     *   the moment the cart is swapped for another game. An archived or partly-uninstalled title
     *   does the same.
     *
     * Enumerating save data finds those, because the OS lists the save whether or not anything is
     * currently installed to play it. It is also cheaper: no mount/unmount probe per title, which
     * was both slow and a devoptab slot churn.
     */
    // Scan one save-data space and append this user's Pokemon saves. Returns false only if the
    // space could not be opened at all; an empty space is a perfectly normal success.
    bool SaveSelectScreen::scanSaveSpace(UserEntry& user, int spaceId, int& scanned, int& forUser) {
        FsSaveDataInfoReader reader;
        Result rc = fsOpenSaveDataInfoReader(&reader, static_cast<FsSaveDataSpaceId>(spaceId));
        if (R_FAILED(rc)) {
            char m[112];
            snprintf(m, sizeof(m), "SaveSelect: cannot read save space %d (rc=0x%08X)", spaceId, (unsigned)rc);
            logInfoToFile(m);
            return false;
        }

        FsSaveDataInfo info[24];
        s64 readCount = 0;
        while (R_SUCCEEDED(fsSaveDataInfoReaderRead(&reader, info, 24, &readCount)) && readCount > 0) {
            for (s64 i = 0; i < readCount; i++) {
                scanned++;
                // Account saves only -- system/temporary/cache entries are not a player's save file.
                if (info[i].save_data_type != FsSaveDataType_Account) continue;
                if (memcmp(&info[i].uid, &user.uid, sizeof(AccountUid)) != 0) continue;
                forUser++;

                const u64 titleId = info[i].application_id;
                GameVersion gv = getGameVersion(titleId);
                if (gv == GameVersion::Invalid) continue;   // not a Pokemon title PKSE knows

                // One tile per game. A title can report more than one save entry (save_data_index),
                // and scanning two spaces can see the same save twice -- listing a game twice would
                // be worse than useless.
                bool dup = false;
                for (const auto& t : user.titles) {
                    if (t.titleId == titleId) { dup = true; break; }
                }
                if (dup) continue;

                char m[128];
                snprintf(m, sizeof(m), "SaveSelect: %s (%016llX) -> listed",
                         getGameVersionName(gv).c_str(), (unsigned long long)titleId);
                logInfoToFile(m);

                TitleEntry t;
                t.titleId = titleId;
                t.label = getGameVersionName(gv);       // short: "盾", "Legends: Z-A", ...
                t.name  = "宝可梦 " + t.label;         // full name (backup dir + downstream compat)
                user.titles.push_back(std::move(t));
            }
        }
        fsSaveDataInfoReaderClose(&reader);
        return true;
    }

    void SaveSelectScreen::loadTitlesForUser(UserEntry& user) {
        int scanned = 0, forUser = 0;

        // `All` is the pseudo-space the header blesses for this reader, and it is what should
        // normally answer. Fall back to the concrete spaces if it is refused, because "found
        // nothing" is precisely the failure this function exists to stop producing.
        if (!scanSaveSpace(user, FsSaveDataSpaceId_All, scanned, forUser)) {
            scanSaveSpace(user, FsSaveDataSpaceId_User,   scanned, forUser);
            scanSaveSpace(user, FsSaveDataSpaceId_SdUser, scanned, forUser);
        }

        // Counts make a missing title diagnosable from the log alone: how many saves the console
        // reported in total, how many belong to this user, and how many were Pokemon titles.
        char summary[192];
        snprintf(summary, sizeof(summary),
                 "SaveSelect: %d save entries on console, %d for %s, %d Pokemon titles listed",
                 scanned, forUser, user.name.c_str(), (int)user.titles.size());
        logInfoToFile(summary);
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

    // Rows of tiles, at MAX_COLS per row.
    int SaveSelectScreen::titleRows() const {
        const UserEntry* u = currentUser();
        const int count = u ? (int)u->titles.size() : 0;
        if (count <= 0) return 0;
        const int cols = titleColumns();
        return (count + cols - 1) / cols;
    }

    /**
     * Move the scroll window as LITTLE as possible to keep the selected tile on screen.
     *
     * `scrollRow` is deliberately STATE, not something recomputed from `titleIndex` each frame. The
     * derived version centred the selection -- `first = selRow - VISIBLE_ROWS/2` -- which reads as
     * paging: with three rows, selecting the bottom row shows rows 1-2, and moving back up to the
     * middle row snapped the view to rows 0-1 even though the middle row was *already visible*.
     * Every vertical move repainted the whole grid.
     *
     * Scrolling only when the selection would otherwise fall outside the window means the view
     * holds still while the cursor moves inside it, and shifts by exactly one row at the edges.
     * With three rows that is: nothing at all between the top two rows, one row down when you enter
     * the bottom row, and one row back up only when you leave the top row.
     */
    void SaveSelectScreen::scrollSelectionIntoView() {
        const int rows = titleRows();
        if (rows <= VISIBLE_ROWS) { scrollRow = 0; return; }   // everything fits; never offset

        const int selRow = titleIndex / titleColumns();
        if (selRow < scrollRow)                            scrollRow = selRow;
        else if (selRow >= scrollRow + VISIBLE_ROWS)       scrollRow = selRow - VISIBLE_ROWS + 1;

        // Clamp for the case the row count shrank under us (switching to a user with fewer saves).
        if (scrollRow > rows - VISIBLE_ROWS) scrollRow = rows - VISIBLE_ROWS;
        if (scrollRow < 0)                   scrollRow = 0;
    }

    void SaveSelectScreen::setUser(int idx) {
        if (users.empty()) return;
        userIndex = (idx % (int)users.size() + (int)users.size()) % (int)users.size();
        titleIndex = 0;
        scrollRow  = 0;   // a different user has a different title count; start at the top
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
            if (kDown & HidNpadButton_Down) {
                // A partial last row still has to be reachable. Straight down when that column
                // exists below; otherwise fall to the final tile rather than refusing to move --
                // with twelve titles the bottom row is just two wide, and Down from the right-hand
                // columns would otherwise do nothing at all.
                if (titleIndex + cols < count)                        titleIndex += cols;
                else if (titleIndex / cols < (count - 1) / cols)      titleIndex = count - 1;
            }
            if ((kDown & HidNpadButton_Up)   && titleIndex - cols >= 0)    titleIndex -= cols;
            if (kDown & HidNpadButton_A) selectCurrentTitle();
            // One place, after every way the selection can move -- stick, D-pad or a tap.
            scrollSelectionIntoView();
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
            const int cols  = titleColumns();
            const int rows  = titleRows();
            const int first = scrollRow;
            const int gridW = cols * TILE_W + (cols - 1) * GAP;
            int startX = (fb.getWidth() - gridW) / 2;
            if (startX < 40) startX = 40;

            // Only the rows inside the window are drawn, and only those get tap targets -- an
            // off-screen tile must not be tappable through whatever is covering it.
            const int firstIdx = first * cols;
            const int lastIdx  = (first + VISIBLE_ROWS) * cols;   // exclusive

            for (int i = firstIdx; i < count && i < lastIdx; i++) {
                int col = i % cols, row = (i / cols) - first;
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

            // Scrollbar to the right of the grid, drawn only when the rows overflow -- same thumb
            // the backup list and the details editor use.
            constexpr int ROW_PITCH = TILE_H + GAP;
            drawScrollbar(fb, startX + gridW + 12, GRID_Y, VISIBLE_ROWS * ROW_PITCH,
                          rows * ROW_PITCH, first * ROW_PITCH);
        }

        // ---- Footer ----
        if (users.size() > 1)
            drawNavBar(fb, "A：选择  |  L/R：切换用户  |  +：退出");
        else
            drawNavBar(fb, "A：选择  |  +：退出");
    }
}
