#include "UI/Dialogs/ItemEditDialog.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Utils/HelperUtilities.h"

using namespace Trainer;
using namespace Utils;

namespace UI {
namespace Dialogs {
    void drawItemEditDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        // Draw semi-transparent overlay (simulate with dark panel)
        int dialogWidth = 500;
        int dialogHeight = 200;
        int dialogX = (fb.getWidth() - dialogWidth) / 2;
        int dialogY = (fb.getHeight() - dialogHeight) / 2;

        // Draw dialog background
        fb.drawFilledRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Panel);
        fb.drawRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Border);

        // Draw title
        fb.drawText(dialogX + 20, dialogY + 20, "编辑道具数量", Colors::Text);
        fb.drawFilledRect(dialogX + 20, dialogY + 45, dialogWidth - 40, 2, Colors::Border);

        // Get item name
        std::string itemName = "未知道具";
        if (screen.selectedCategory >= 0 && screen.selectedCategory < static_cast<int>(screen.trainer.items.size())) {
            const auto& pouch = screen.trainer.items[screen.selectedCategory];
            if (screen.selectedItemIndex >= 0 && screen.selectedItemIndex < static_cast<int>(pouch.size())) {
                itemName = getItemName(pouch[screen.selectedItemIndex].itemId);
            }
        }

        // Draw item name
        fb.drawText(dialogX + 20, dialogY + 65, "道具: " + itemName, Colors::Text);

        // Draw current value (large and centered)
        char valueText[32];
        snprintf(valueText, sizeof(valueText), "数量: %d", screen.itemEditDialogValue);
        fb.drawText(dialogX + 150, dialogY + 95, valueText, Colors::Yellow);
    }
}
}