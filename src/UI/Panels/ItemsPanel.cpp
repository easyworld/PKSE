#include <algorithm>

#include "UI/Panels/ItemsPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Trainer/Inventory9LZA.h"
#include "Trainer/Inventory8SWSH.h"
#include "Enums/GameVersion.h"
#include "Utils/HelperUtilities.h"

using namespace Trainer;
using namespace Enums;
using namespace Utils;

namespace UI {
namespace Panels {
    void drawItems(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height) {
        fb.drawFilledRect(x, y, width, height, Colors::Panel);
        fb.drawRect(x, y, width, height, Colors::Border);

        // Get current pouch info based on game version
        GameVersion gameGroup = screen.trainer.getGameGroup();
        const char* pouchName = nullptr;

        if (gameGroup == GameVersion::ZA) {
            // Gen 9 Legends Z-A uses PouchType9
            PouchType9LZA pouchType = static_cast<PouchType9LZA>(screen.selectedCategory);
            const PouchInfo9LZA& info = getPouchInfo9LZA(pouchType);
            pouchName = info.name;
        } else {
            // Gen 8 use PouchType
            PouchType8SWSH pouchType = static_cast<PouchType8SWSH>(screen.selectedCategory);
            const PouchInfo8SWSH& info = getPouchInfo8SWSH(pouchType);
            pouchName = info.name;
        }

        // Header with category name
        std::string headerText = std::string("道具 - ") + pouchName;
        // if (screen.detailViewActive) {
        //     headerText += " [DETAIL VIEW]";
        // }
        fb.drawText(x + 20, y + 20, headerText, Colors::Text);
        fb.drawFilledRect(x + 20, y + 45, width - 40, 2, Colors::Border);

        // Get items from trainer
        if (screen.selectedCategory >= 0 && screen.selectedCategory < static_cast<int>(screen.trainer.items.size())) {
            const auto& pouch = screen.trainer.items[screen.selectedCategory];

            if (pouch.empty()) {
                fb.drawText(x + 20, y + 80, "此类别中没有道具", Colors::TextDim);
                return;
            }

            // Calculate layout based on detail view state
            int lineHeight = 20;
            int columnWidth = (width - 60) / 2;  // Dynamic column width based on panel width
            int itemsPerColumn = (height - 120) / lineHeight;  // Dynamic items per column based on height
            int itemsPerPage = itemsPerColumn * 2;  // Two columns

            // Display item count and page info
            int totalPages = (pouch.size() + itemsPerPage - 1) / itemsPerPage;
            std::string countText = std::to_string(pouch.size()) + " items";
            if (screen.detailViewActive && totalPages > 1) {
                countText += " (Page " + std::to_string(screen.currentPage + 1) + "/" + std::to_string(totalPages) + ")";
            }
            fb.drawText(x + 20, y + 70, countText, Colors::TextDim);

            // Calculate which items to display based on pagination
            size_t startIdx = screen.detailViewActive ? (screen.currentPage * itemsPerPage) : 0;
            size_t endIdx = screen.detailViewActive ? std::min(startIdx + itemsPerPage, pouch.size()) : std::min(pouch.size(), (size_t)itemsPerPage);

            // Draw items in two columns
            for (size_t i = startIdx; i < endIdx; i++) {
                const InventoryItem& item = pouch[i];

                // Calculate display index (relative to current page)
                size_t displayIdx = i - startIdx;

                // Determine column (0 = left, 1 = right)
                int column = (static_cast<int>(displayIdx) >= itemsPerColumn) ? 1 : 0;
                int itemInColumn = (static_cast<int>(displayIdx) >= itemsPerColumn) ? (displayIdx - itemsPerColumn) : displayIdx;

                // Calculate position
                int colX = x + 20 + (column * columnWidth);
                int colY = y + 100 + (itemInColumn * lineHeight);

                // Highlight selected item in detail view
                if (screen.detailViewActive && static_cast<int>(i) == screen.selectedItemIndex) {
                    fb.drawFilledRect(colX - 5, colY - 2, columnWidth - 10, lineHeight - 2, Colors::Selected);
                }

                // Format: "Item Name x5" or with favorite marker
                char itemText[64];
                const char* marker = item.isFavorite ? "*" : " ";
                const char* cursor = (screen.detailViewActive && static_cast<int>(i) == screen.selectedItemIndex) ? "" : " ";
                snprintf(itemText, sizeof(itemText), "%s%s x%-3d %s",
                        cursor, marker, item.count, getItemName(item.itemId));

                Color textColor = item.isNew ? Colors::Yellow : Colors::Text;
                fb.drawText(colX, colY, itemText, textColor);
            }

            // Show pagination hint if there are more items and not in detail view
            if (!screen.detailViewActive && static_cast<int>(pouch.size()) > itemsPerPage) {
                std::string moreText = "... and " + std::to_string(pouch.size() - itemsPerPage) + " more items";
                fb.drawText(x + 363, y + height - 30, moreText, Colors::TextDim);
            }
        } else {
            fb.drawText(x + 20, y + 80, "Invalid category", Colors::TextDim);
        }
    }
}
}
