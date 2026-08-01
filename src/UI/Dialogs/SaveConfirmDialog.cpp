#include "UI/Dialogs/SaveConfirmDialog.h"
#include "UI/Dialogs/DialogFrame.h"
#include "UI/Dialogs/EditControls.h"   // drawEditChoiceButton -- on-button controller glyphs
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Globals.h"

namespace UI {
namespace Dialogs {
    namespace {
        // Leaf folder name of a backup path -- that IS the backup's name in the picker list, the
        // same way it is in the backup selection screen.
        std::string leafOf(const std::string& path) {
            const size_t slash = path.find_last_of('/');
            return (slash == std::string::npos) ? path : path.substr(slash + 1);
        }
    }

    void drawSaveConfirmDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        constexpr int w = 560;

        // Exiting with unsaved changes is a different question ("要放弃这些更改吗？"), not a
        // destination choice, so it keeps the plain two-button form.
        if (screen.exitingWithUnsavedChanges) {
            constexpr int h = 248;
            const int x = (fb.getWidth() - w) / 2, y = (fb.getHeight() - h) / 2;
            int cy = drawDialogFrame(fb, x, y, w, h, "未保存的更改", Colors::Warning);
            fb.drawText(x + 24, cy,      "你有尚未保存的更改。", Colors::Text);
            fb.drawText(x + 24, cy + 28, "继续操作将丢失这些更改。", Colors::TextDim);

            // Buttons carry their glyph (id 0 = Cancel/B, id 1 = Discard & Exit/A), no guide line.
            screen.touchButtons.clear();
            const int cbh = TouchTargetMin, cby = y + h - cbh - 16;
            const int cbw = (w - 48 - 16) / 2;
            drawEditChoiceButton(screen, fb, x + 24,           cby, cbw, cbh, "B", "取消",         0);
            drawEditChoiceButton(screen, fb, x + w - 24 - cbw, cby, cbw, cbh, "A", "放弃并退出", 1);
            return;
        }

        // --- Destination picker ---------------------------------------------------------
        const int rows = screen.saveDestCount();
        constexpr int rowH = 58, rowGap = 8;
        // The illegal-values notice is a LINE here, not another dialog. This dialog is already a
        // confirm/cancel, so folding the warning in tells the user without adding a step.
        const int warnH = screen.illegalDataWritten ? 26 : 0;
        const int h = 168 + warnH + rows * (rowH + rowGap);
        const int x = (fb.getWidth() - w) / 2, y = (fb.getHeight() - h) / 2;

        int cy = drawDialogFrame(fb, x, y, w, h, "保存更改", Colors::Text);
        fb.drawText(x + 24, cy,
                    screen.hasUnsavedChanges ? "要写入到哪里？"
                                             : "没有任何更改，仍要写入吗？",
                    screen.hasUnsavedChanges ? Colors::Text : Colors::TextDim);
        if (screen.illegalDataWritten) {
            fb.drawText(x + 24, cy + 24,
                        "包含游戏判定为非法的数值。",
                        Color(235, 120, 120), TextStyle::Caption);
            cy += warnH;
        }

        const std::string backupName = leafOf(screen.backupDir);
        const char* titles[3] = { "当前备份", "新备份……", "游戏存档" };
        const std::string subs[3] = {
            backupName,
            "使用键盘命名",
            // Same destination, very different act depending on where this session came from.
            screen.loadedFromCart ? "写回" + screen.titleName
                                  : "用此备份覆盖游戏中的实时存档",
        };

        screen.touchButtons.clear();
        const int rx = x + 20, rw = w - 40;
        int ry = cy + 34;
        for (int i = 0; i < rows; ++i) {
            const bool sel = (screen.saveDestIndex == i);
            if (sel) fb.drawSelectionHighlight(rx, ry, rw, rowH);
            else     fb.drawFilledRoundedRect(rx, ry, rw, rowH, 10, Colors::PanelAlt);

            // Red only when writing to the game would DESTROY something: a backup-sourced session
            // overwriting live progress. Saving a cart session back to its own cart is routine and
            // shouldn't be dressed up as a hazard.
            const bool danger = (i == TrainerViewScreen::DestGameSave) && !screen.loadedFromCart;
            fb.drawText(rx + 18, ry + 7, titles[i],
                        danger ? Color(235, 120, 120) : Colors::Text, TextStyle::Body);
            fb.drawText(rx + 18, ry + 32, subs[i], Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.push_back({ i, rx, ry, rw, rowH });
            ry += rowH + rowGap;
        }

        drawDialogFooter(fb, x, y, w, h, "上/下：选择  |  A：保存  |  B：取消");
    }

    void drawSaveInjectConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        constexpr int w = 620, h = 268;
        const int x = (fb.getWidth() - w) / 2, y = (fb.getHeight() - h) / 2;

        int cy = drawDialogFrame(fb, x, y, w, h, "写入游戏存档吗？", Color(235, 120, 120));
        fb.drawText(x + 24, cy,
                    "这将替换" + screen.titleName + "的存档数据", Colors::Text);
        fb.drawText(x + 24, cy + 26,
                    "内容来自备份“" + leafOf(screen.backupDir) + "”及当前编辑。", Colors::Text);
        // Say plainly what is at risk. The user may have loaded a backup from weeks ago, in which
        // case this rolls their game back -- and the dialog is the only place that can warn them.
        fb.drawText(x + 24, cy + 60,
                    "该备份之后的所有游戏进度都将丢失。", Color(235, 120, 120));
        fb.drawText(x + 24, cy + 86,
                    "无论如何都会写入备份本身。", Colors::TextDim, TextStyle::Caption);

        screen.touchButtons.clear();
        const int bw = (w - 60) / 2, bh = 52, by = y + h - 48 - bh - 8;
        // Glyph ON each button (B: Cancel, A: Write to game); the destructive action stays red.
        drawEditChoiceButton(screen, fb, x + 20,      by, bw, bh, "B", "取消",        0);
        drawEditChoiceButton(screen, fb, x + 40 + bw, by, bw, bh, "A", "写入游戏", 1,
                             Color(160, 60, 60), Colors::White);
    }
}
}
