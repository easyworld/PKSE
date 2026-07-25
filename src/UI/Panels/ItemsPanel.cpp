#include <algorithm>
#include <string>
#include <vector>

#include "UI/Panels/ItemsPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Trainer/Inventory9LZA.h"
#include "Trainer/Inventory9SV.h"
#include "Trainer/Inventory8LA.h"
#include "Trainer/Inventory8BDSP.h"
#include "Trainer/Inventory8SWSH.h"
#include "Trainer/Inventory7LGPE.h"
#include "Trainer/Inventory3FRLG.h"
#include "Enums/GameVersion.h"
#include "Utils/HelperUtilities.h"
#include "Names/MoveNames.h"
#include "Names/TMMoves.h"
#include "Names/ItemNames.h"

using namespace Trainer;
using namespace Enums;
using namespace Utils;

namespace UI {
namespace Panels {
    void drawItems(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height) {
        fb.drawFilledRoundedRect(x, y, width, height, 16, Colors::Panel);
        fb.drawRoundedRect(x, y, width, height, 16, Colors::Border, 1);

        // Header band + pouch name.
        constexpr int hH = 46;
        fb.drawFilledRoundedRect(x, y, width, hH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + hH - 16, width, 16, Colors::AccentDim);
        GameVersion gameGroup = screen.trainer.getGameGroup();
        const char* pouchName = nullptr;
        if (gameGroup == GameVersion::ZA)      pouchName = getPouchInfo9LZA(static_cast<PouchType9LZA>(screen.selectedCategory)).name;
        else if (gameGroup == GameVersion::SV) pouchName = getPouchInfo9SV(static_cast<PouchType9SV>(screen.selectedCategory)).name;
        else if (gameGroup == GameVersion::PLA) pouchName = getPouchInfo8LA(static_cast<PouchType8LA>(screen.selectedCategory)).name;
        else if (gameGroup == GameVersion::BDSP) pouchName = getPouchInfo8BDSP(static_cast<PouchType8BDSP>(screen.selectedCategory)).name;
        else if (gameGroup == GameVersion::GG) pouchName = getPouchInfo7LGPE(static_cast<PouchType7LGPE>(screen.selectedCategory)).name;
        else if (gameGroup == GameVersion::FRLG) pouchName = getPouchInfo3FRLG(static_cast<PouchType3FRLG>(screen.selectedCategory)).name;
        else                                   pouchName = getPouchInfo8SWSH(static_cast<PouchType8SWSH>(screen.selectedCategory)).name;
        fb.drawText(x + 22, y + (hH - fb.lineHeight(TextStyle::Heading)) / 2, std::string("道具 - ") + pouchName, Colors::Text, TextStyle::Heading);

        screen.touchButtons.clear();

        if (screen.selectedCategory < 0 || screen.selectedCategory >= static_cast<int>(screen.trainer.items.size())) {
            fb.drawText(x + 24, y + hH + 30, "无效分类", Colors::TextDim);
            return;
        }
        const auto& pouch = screen.trainer.items[screen.selectedCategory];
        // Only visible items (count > 0); "已有但为空" slots are hidden but kept in the data.
        std::vector<int> visible = screen.visibleItemIndices();
        if (visible.empty()) {
            fb.drawText(x + 24, y + hH + 30, "此分类中没有道具", Colors::TextDim);
            return;
        }
        const int total = static_cast<int>(visible.size());

        // Single-column touch-friendly tiles (this geometry mirrors the nav math in
        // TrainerViewScreen::update() — keep them in sync).
        constexpr int rowPitch = 52, tileH = 46;
        const int itemsPerPage = (height - 106) / rowPitch;
        const int totalPages = (total + itemsPerPage - 1) / itemsPerPage;
        const int tileW = std::min(width - 60, 860);
        const int tileX = x + (width - tileW) / 2;

        std::string countText = std::to_string(total) + " 个道具";
        if (totalPages > 1) countText += "      第 " + std::to_string(screen.currentPage + 1) + " / " + std::to_string(totalPages);
        fb.drawText(tileX, y + hH + 10, countText, Colors::TextDim, TextStyle::Caption);

        const int startIdx = screen.currentPage * itemsPerPage;
        const int endIdx = std::min(startIdx + itemsPerPage, total);
        int ry = y + hH + 40;
        for (int i = startIdx; i < endIdx; ++i) {
            const InventoryItem& item = pouch[visible[i]];
            const bool selected = screen.detailViewActive && i == screen.selectedItemIndex;
            fb.drawSoftShadow(tileX, ry, tileW, tileH, tileH / 2);
            fb.drawFilledRoundedRect(tileX, ry, tileW, tileH, 12, selected ? Colors::Primary : Colors::PanelAlt);
            const Color nameCol = selected ? Colors::PrimaryText : (item.isNew ? Colors::Accent : Colors::Text);
            int nx = tileX + 22;
            if (item.isFavorite) { fb.drawSymbol(nx, ry + (tileH - 20) / 2, "\xE2\x98\x85", Colors::Yellow); nx += 24; }
            const char* itemName = (gameGroup == GameVersion::FRLG)
                ? Names::getItemNameG3(item.itemId)   // Gen 3 ids differ -> convert then name
                : getItemName(item.itemId);
            fb.drawText(nx, ry + (tileH - fb.lineHeight(TextStyle::Body)) / 2, itemName, nameCol, TextStyle::Body);
            // TM/HM/TR items: show the move the machine teaches (dim, after the item name).
            if (uint16_t tmMove = Names::getTMMove(gameGroup, item.itemId)) {
                int iw, ih; fb.measureText(itemName, iw, ih, TextStyle::Body);
                const Color moveCol = selected ? Colors::PrimaryText : Colors::TextDim;
                fb.drawText(nx + iw + 14, ry + (tileH - fb.lineHeight(TextStyle::Body)) / 2, Names::getMoveName(tmMove), moveCol, TextStyle::Body);
            }
            std::string cnt = "\xC3\x97" + std::to_string(item.count);  // ×N
            int cw, ch; fb.measureText(cnt, cw, ch, TextStyle::Body);
            fb.drawText(tileX + tileW - 24 - cw, ry + (tileH - ch) / 2, cnt, selected ? Colors::PrimaryText : Colors::TextDim, TextStyle::Body);
            screen.touchButtons.push_back({ i, tileX, ry, tileW, tileH });  // id = absolute item index
            ry += rowPitch;
        }
    }
}
}
