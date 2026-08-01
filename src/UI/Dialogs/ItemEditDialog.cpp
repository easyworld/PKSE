#include <string>
#include <vector>

#include "UI/Dialogs/ItemEditDialog.h"
#include "UI/Dialogs/DialogFrame.h"
#include "UI/Dialogs/EditControls.h"   // shared step buttons + A/B choice buttons
#include "UI/ScreenChrome.h"           // drawGlyphButton (remove-confirm buttons)
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Names/ItemNames.h"     // getItemNameG3 -- Gen 3 uses a separate item id space
#include "Enums/GameVersion.h"
#include "Utils/HelperUtilities.h"

using namespace Trainer;
using namespace Utils;

namespace UI {
namespace Dialogs {
    void drawItemEditDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        constexpr int w = 560, h = 360;
        const int x = (fb.getWidth() - w) / 2;
        const int y = (fb.getHeight() - h) / 2;

        int cy = drawDialogFrame(fb, x, y, w, h, "编辑道具", Colors::Text);

        // Resolve the item name via the visible list (selectedItemIndex indexes visible items).
        // Gen 3 ids are a SEPARATE id space from the modern one -- resolving a FireRed bag item
        // through the modern table named Potion (g3 13) "Dusk Ball" (modern 13). The list panel
        // already branched; this dialog did not, so the two disagreed on screen.
        const bool g3Items = (screen.trainer.getGameGroup() == Enums::GameVersion::FRLG);
        std::string itemName = "未知道具";
        if (screen.selectedCategory >= 0 && screen.selectedCategory < static_cast<int>(screen.trainer.items.size())) {
            const auto& pouch = screen.trainer.items[screen.selectedCategory];
            std::vector<int> visible = screen.visibleItemIndices();
            if (screen.selectedItemIndex >= 0 && screen.selectedItemIndex < static_cast<int>(visible.size())) {
                const uint16_t id = pouch[visible[screen.selectedItemIndex]].itemId;
                itemName = g3Items ? Names::getItemNameG3(id) : getItemName(id);
            }
        }

        screen.touchButtons.clear();

        // ITEM label + the pouch's max stack (so the cap is visible instead of the value just refusing
        // to rise -- e.g. a key item stuck at "1 / max 1").
        fb.drawText(x + 24, cy, "道具", Colors::TextDim, TextStyle::Caption);
        {
            const std::string cap = "上限 " + std::to_string(screen.currentItemMaxCount());
            int cw, ch; fb.measureText(cap, cw, ch, TextStyle::Caption);
            fb.drawText(x + w - 24 - cw, cy, cap, Colors::TextDim, TextStyle::Caption);
        }

        // Item drop-down: shows the current item; X (or a tap) opens the picker to CHANGE its type
        // (Potion -> Super Potion). It is a real touch target (id 40) plus the X face button.
        const int ddX = x + 24, ddY = cy + 20, ddW = w - 48, ddH = 42;
        fb.drawFilledRoundedRect(ddX, ddY, ddW, ddH, 8, Colors::PanelAlt);
        fb.drawRoundedRect(ddX, ddY, ddW, ddH, 8, Colors::Border, 1);
        fb.drawText(ddX + 16, ddY + (ddH - fb.lineHeight(TextStyle::Body)) / 2, itemName, Colors::Text);
        fb.drawSymbol(ddX + ddW - 28, ddY + ddH / 2 - 8, "\xE2\x96\xBC", Colors::TextDim, TextStyle::Caption);  // v
        { const int gw = buttonGlyphWidth(fb, "X");
          buttonGlyph(fb, ddX + ddW - 40 - gw, ddY + ddH / 2, "X", false); }
        screen.touchButtons.push_back({40, ddX, ddY, ddW, ddH});

        // Amount, large and centered in the accent color.
        std::string amount = std::to_string(screen.itemEditDialogValue);
        int aw, ah; fb.measureText(amount, aw, ah, TextStyle::Title);
        fb.drawText(x + (w - aw) / 2, cy + 76, amount, Colors::Accent, TextStyle::Title);

        // Step buttons (ZL -100 | L -10 | < -1 | > +1 | R +10 | ZR +100), then Cancel / Remove / Confirm
        // labelled with B / Y / A.
        const int cbh = TouchTargetMin, cby = y + h - cbh - 14;
        drawEditStepRow(screen, fb, x, w, cby - 58 - 14, 58);

        const int cbw = 160, gap = 16;
        drawEditChoiceButton(screen, fb, x + 24,             cby, cbw, cbh, "B", "取消",  0);
        drawEditChoiceButton(screen, fb, x + 24 + cbw + gap, cby, cbw, cbh, "Y", "移除",  2);
        drawEditChoiceButton(screen, fb, x + w - 24 - cbw,   cby, cbw, cbh, "A", "确认", 1);
    }

    // Confirm before deleting the selected item from the Items list. Mirrors the storage
    // release confirm: red frame, B = Cancel (id 0), A = Remove (id 1). The delete itself runs in
    // TrainerViewScreen's itemRemoveConfirmActive handler.
    void drawItemRemoveConfirm(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        const bool g3Items = (screen.trainer.getGameGroup() == Enums::GameVersion::FRLG);
        std::string itemName = "此道具";
        if (screen.selectedCategory >= 0 && screen.selectedCategory < static_cast<int>(screen.trainer.items.size())) {
            const auto& pouch = screen.trainer.items[screen.selectedCategory];
            std::vector<int> visible = screen.visibleItemIndices();
            if (screen.selectedItemIndex >= 0 && screen.selectedItemIndex < static_cast<int>(visible.size())) {
                const uint16_t id = pouch[visible[screen.selectedItemIndex]].itemId;
                itemName = g3Items ? Names::getItemNameG3(id) : getItemName(id);
            }
        }

        constexpr int w = 540, h = 226;
        const int x = (fb.getWidth() - w) / 2;
        const int y = (fb.getHeight() - h) / 2;
        int cy = drawDialogFrame(fb, x, y, w, h, "移除道具", Colors::Red);
        fb.drawText(x + 28, cy, "移除“" + itemName + "”？", Colors::Text);
        fb.drawText(x + 28, cy + 34, "该道具将从此口袋移除。", Colors::TextDim, TextStyle::Caption);

        screen.touchButtons.clear();
        const int bw = 190, bh = TouchTargetMin, by = y + h - bh - 18;
        const int remX = x + w - bw - 20;      // right = Remove (id 1 -> A)
        const int cancelX = remX - bw - 16;    // left  = Cancel (id 0 -> B)
        drawGlyphButton(fb, cancelX, by, bw, bh, "B", "取消", Colors::PanelAlt);
        screen.touchButtons.push_back({0, cancelX, by, bw, bh});
        drawGlyphButton(fb, remX,    by, bw, bh, "A", "移除", Colors::Red, Colors::White);
        screen.touchButtons.push_back({1, remX, by, bw, bh});
    }
}
}
