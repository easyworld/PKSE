#ifndef UI_SAVE_SELECT_SCREEN_H
#define UI_SAVE_SELECT_SCREEN_H

#include <vector>
#include <string>

#include <switch.h>

#include "UI/UIScreen.h"
#include "UI/PKSEFramebuffer.h"

namespace UI {
    // JKSV-style combined user + title picker. Shows the selected user's avatar + name at the top
    // and a grid of that user's supported Pokemon game icons below. Replaces the old two-step
    // UserSelection -> TitleSelection flow. Switch users with the L/R shoulders (or by tapping a
    // user chip) when more than one account exists. Only titles PKSE supports, and only ones the
    // user actually has a save for, appear.
    class SaveSelectScreen : public UIScreen {
    public:
        SaveSelectScreen();
        void update(const PadState& pad, const TouchInput& touch) override;
        void draw(PKSEFramebuffer& fb) override;
        bool shouldExit() const override { return exitRequested; }

        bool hasSelectedTitle() const { return titleSelected; }
        AccountUid getSelectedUser() const { return selectedUserUid; }
        u64 getSelectedTitleId() const { return selectedTitleId; }
        const std::string& getSelectedTitleName() const { return selectedTitleName; }

    private:
        struct TitleEntry {
            u64 titleId;
            std::string name;    // full "Pokemon X" name (used for backup dir + downstream)
            std::string label;   // short display name under the icon (e.g. "盾")
        };
        struct UserEntry {
            AccountUid uid;
            std::string name;
            std::vector<TitleEntry> titles;
        };
        struct HitRect { int x, y, w, h, idx; };

        std::vector<UserEntry> users;
        int userIndex = 0;
        int titleIndex = 0;

        bool titleSelected = false;
        bool exitRequested = false;
        AccountUid selectedUserUid{};
        u64 selectedTitleId = 0;
        std::string selectedTitleName;

        // Tap targets captured during draw(), hit-tested on the next update().
        std::vector<HitRect> titleRects;
        std::vector<HitRect> userRects;

        void loadUsers();
        void loadTitlesForUser(UserEntry& user);
        void setUser(int idx);
        void selectCurrentTitle();

        const UserEntry* currentUser() const;
        int titleColumns() const;   // grid columns for the current user's title count (<= 5)
    };
}

#endif
