#include <cstdint>
#include <string>

#include "UI/Dialogs/PickerDialog.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h" // drawScrollbar
#include "UI/ListSearch.h"   // the shared search box
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"     // getNatureName / getAbilityName
#include "Names/MoveNames.h"     // getMoveName / getMoveCount
#include "Names/ItemNames.h"     // getItemNameFor -- every generation numbers items differently
#include "Pokemon/Gen1Tables.h"  // getItemNameGen1
#include "Enums/Ball.h"          // getBallName + MAX_BALL_GEN7 (Hisui-ball disambiguation)
#include "Enums/LanguageID.h"    // getLanguageName
#include "Enums/GameVersion.h"   // getGameVersionName (Origin picker)
#include "Save/TitleFolder.h"    // localizedGameVersionName
#include "Names/LocationNames.h" // getMetLocationName (Met Location picker)
#include "Names/FormNames.h"     // getFormName (Form picker)

using namespace Trainer;

namespace UI
{
    namespace Dialogs
    {

        static const char *const GENDER_LABELS[3] = {"雄性", "雌性", "无性别"};

        int pickerOptionCount(PickerKind kind)
        {
            switch (kind)
            {
            case PickerKind::Nature:
                return 25;
            case PickerKind::Gender:
                return 3;
            case PickerKind::Move:
                return static_cast<int>(Names::getMoveCount());
            case PickerKind::Item:
                return static_cast<int>(getItemCount());
            case PickerKind::ItemG3:
                return static_cast<int>(Names::getItemCountG3());
            case PickerKind::ItemG1:
                return static_cast<int>(::Pokemon::MAX_ITEM_GEN1) + 1;
            case PickerKind::Level:
                return 100;
            case PickerKind::Friendship:
                return 256;
            case PickerKind::Ball:
                return 38; // 0=(none) .. 37=Origin Ball
            case PickerKind::Ability:
                return 311; // AbilityNames covers ids 0-310 (through Gen 9)
            case PickerKind::Language:
                return 11; // 0-10 (6 is unused, shown as "-")
            case PickerKind::Origin:
                return 53; // version ids 0-52 (getGameVersionName)
            case PickerKind::MetLevel:
                return 100; // met level 1-100
            case PickerKind::MetLocation:
                return 0; // ids supplied via pickerOrder (caller sets count)
            case PickerKind::Form:
                return 0; // form count is species-dependent (caller sets count)
            case PickerKind::StatNature:
                return 25; // same 25 natures as the real nature
            case PickerKind::Species:
                return 1026; // ids 0-1025 (0 = None)
            // Pouch lists are supplied through pickerOrder, so the caller sets pickerCount.
            case PickerKind::PouchItem:
            case PickerKind::PouchItemG1:
            case PickerKind::PouchItemG3:
                return 0;
            }
            return 0;
        }

        const char *pickerOptionLabel(PickerKind kind, int index)
        {
            switch (kind)
            {
            case PickerKind::Nature:
                return getNatureName(static_cast<uint8_t>(index));
            case PickerKind::Gender:
                return (index >= 0 && index < 3) ? GENDER_LABELS[index] : "?";
            case PickerKind::Move:
                return Names::getMoveName(static_cast<uint16_t>(index));
            case PickerKind::Item:
                return getItemName(static_cast<uint16_t>(index));
            case PickerKind::ItemG3:
                return Names::getItemNameG3(static_cast<uint16_t>(index));
            case PickerKind::ItemG1:
                return ::Pokemon::getItemNameGen1(static_cast<uint8_t>(index));
            case PickerKind::Level:
            {
                static std::string text;
                text = std::to_string(index + 1);
                return text.c_str();
            }
            case PickerKind::Friendship:
            {
                static std::string text;
                text = std::to_string(index);
                return text.c_str();
            }
            case PickerKind::Ball:
                return Enums::getBallName(static_cast<uint8_t>(index));
            case PickerKind::Ability:
                return getAbilityName(static_cast<uint16_t>(index));
            case PickerKind::Language:
                return Enums::getLanguageName(static_cast<uint8_t>(index));
            case PickerKind::Origin:
            {
                static std::string text;
                text = Save::localizedGameVersionName(static_cast<Enums::GameVersion>(index));
                return text.c_str();
            }
            case PickerKind::MetLevel:
            {
                static std::string text;
                text = std::to_string(index + 1);
                return text.c_str();
            }
            case PickerKind::MetLocation:
                return ""; // labelled in drawPickerDialog (needs the origin version)
            case PickerKind::Form:
                return ""; // labelled in drawPickerDialog (needs the species)
            case PickerKind::StatNature:
                return getNatureName(static_cast<uint8_t>(index));
            case PickerKind::Species:
                return getSpeciesName(static_cast<uint16_t>(index));
            case PickerKind::PouchItem:
                return getItemName(static_cast<uint16_t>(index));
            case PickerKind::PouchItemG3:
                return Names::getItemNameG3(static_cast<uint16_t>(index));
            case PickerKind::PouchItemG1:
                return ::Pokemon::getItemNameGen1(static_cast<uint8_t>(index));
            }
            return "?";
        }

        const char *pickerTitle(PickerKind kind)
        {
            switch (kind)
            {
            case PickerKind::Nature:
                return "选择性格";
            case PickerKind::Gender:
                return "选择性别";
            case PickerKind::Move:
                return "选择招式";
            case PickerKind::Item:
            case PickerKind::ItemG1:
            case PickerKind::ItemG3:
                return "选择携带道具";
            case PickerKind::Level:
                return "选择等级";
            case PickerKind::Friendship:
                return "选择亲密度";
            case PickerKind::Ball:
                return "选择精灵球";
            case PickerKind::Ability:
                return "选择特性";
            case PickerKind::Language:
                return "选择语言";
            case PickerKind::Origin:
                return "选择初训家游戏";
            case PickerKind::MetLevel:
                return "选择相遇等级";
            case PickerKind::MetLocation:
                return "选择相遇地点";
            case PickerKind::Form:
                return "选择形态";
            case PickerKind::StatNature:
                return "选择能力性格（薄荷）";
            case PickerKind::Species:
                return "选择种类";
            case PickerKind::PouchItem:
            case PickerKind::PouchItemG1:
            case PickerKind::PouchItemG3:
                return "向口袋添加道具";
            }
            return "选择";
        }

        std::string pickerRowLabel(const TrainerViewScreen &screen, PickerKind kind, int value)
        {
            if (kind == PickerKind::MetLocation)
                return Names::getMetLocationName(screen.pickerMetVersion, static_cast<uint16_t>(value));
            if (kind == PickerKind::Form)
            {
                const char *formName = Names::getFormName(screen.pickerFormSpecies,
                                                          static_cast<uint8_t>(value));
                if (formName[0] != '\0')
                    return formName;
                return value == 0 ? std::string("普通形态") : ("形态 " + std::to_string(value));
            }
            if (kind == PickerKind::Ball && value >= static_cast<int>(Enums::Ball::LAPoke))
            {
                // BOTH BALL NUMBERINGS CAN BE IN ONE LIST. BDSP, Scarlet/Violet and Legends: Z-A
                // each hold a Hisui ball HOME carried in, so their picker runs 1-37 and four names
                // land twice -- Poke, Great and Ultra (4/28, 3/29, 2/30) and Heavy (20/34) -- because
                // the games really do call both of each by the same name. Unqualified, those rows
                // are a coin flip. Qualify the Hisui half, and only where the ambiguity is actually
                // on screen: the Legends: Arceus list offers no modern ball to be confused with, and
                // the Strange Ball needs nothing because its name is its own.
                bool listMixesNumberings = false;
                for (int offeredBall : screen.pickerOrder)
                {
                    if (offeredBall <= Enums::MAX_BALL_GEN7)
                    {
                        listMixesNumberings = true;
                        break;
                    }
                }
                if (listMixesNumberings)
                {
                    return std::string(Enums::getBallName(static_cast<uint8_t>(value))) + " (Hisui)";
                }
            }
            const char *label = pickerOptionLabel(kind, value);
            return label != nullptr ? std::string(label) : std::string();
        }

        void drawPickerDialog(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const int screenWidth = framebuffer.getWidth(), H = framebuffer.getHeight();
            const PickerKind kind = screen.pickerKind;
            // Rows the user can see and move through: the filtered ones while a query is set.
            const int matchCount = screen.pickerVisibleCount();
            const int count = matchCount > 0 ? matchCount : 1;
            int selectedIndex = screen.pickerSel;
            if (selectedIndex < 0)
                selectedIndex = 0;
            if (selectedIndex >= count)
                selectedIndex = count - 1;

            // Dim behind + centered panel.
            framebuffer.drawFilledRect(0, 0, screenWidth, H, Color(0, 0, 0, 150));
            const int panelWidth = 560, ph = H - 120;
            const int panelX = (screenWidth - panelWidth) / 2, py = 60;
            framebuffer.drawFilledRoundedRect(panelX, py, panelWidth, ph, 16, Colors::Panel);
            framebuffer.drawRoundedRect(panelX, py, panelWidth, ph, 16, Colors::Accent, 2);

            // Title + position caption. A pouch picker opened to change an existing item's type says so
            // rather than "向口袋添加道具".
            const char *title = (screen.itemPickerReplace &&
                                 (kind == PickerKind::PouchItem || kind == PickerKind::PouchItemG3))
                                    ? "更换道具"
                                    : pickerTitle(kind);
            framebuffer.drawText(panelX + 20, py + 16, title, Colors::Text, TextStyle::Heading);
            {
                std::string position = std::to_string(matchCount == 0 ? 0 : selectedIndex + 1) +
                                  " / " + std::to_string(matchCount);
                int previewWidth, phi;
                framebuffer.measureText(position, previewWidth, phi, TextStyle::Caption);
                framebuffer.drawText(panelX + panelWidth - 20 - previewWidth, py + 22, position, Colors::TextDim,
                                     TextStyle::Caption);
            }
            framebuffer.drawHDivider(panelX + 20, py + 52, panelWidth - 40);

            drawListSearchBox(framebuffer, panelX + 12, py + 60, panelWidth - 24, screen.pickerSearch,
                              matchCount, screen.pickerCount);

            // Scrollable list window centered on the selection.
            const int rowH = 40;
            const int listTop = py + 60 + listSearchBoxHeight() + 4, listBottom = py + ph - 48;
            int visible = (listBottom - listTop) / rowH;
            if (visible < 1)
                visible = 1;
            int first = selectedIndex - visible / 2;
            if (first > count - visible)
                first = count - visible;
            if (first < 0)
                first = 0;

            screen.touchButtons.clear();
            // the Ability picker reorders its options (legal abilities first) via screen.pickerOrder,
            // and the legal prefix renders green. Form and Gender filter rather than reorder (forms the
            // game can't hold are dropped; so are genders the species can't be), but they need the same
            // row -> value indirection. Every other kind stays identity-indexed (row == value).
            //
            // A kind that fills pickerOrder and is missing from this list is the sharp edge: labels would
            // keep using the row index while the write uses pickerOrder, naming one value and writing
            // another with nothing on screen to show it.
            const bool reorder = screen.pickerUsesOrder();
            for (int index = 0; index < visible && (first + index) < matchCount; ++index)
            {
                const int entryIndex = first + index;                       // visible row
                const int row = screen.pickerRowForVisibleIndex(entryIndex); // row in the full list
                if (row < 0)
                    continue;
                const int value = (reorder && row < static_cast<int>(screen.pickerOrder.size()))
                                    ? screen.pickerOrder[row]
                                    : row;
                const int rectY = listTop + index * rowH;
                const bool isSelected = (entryIndex == selectedIndex);
                if (isSelected)
                {
                    framebuffer.drawFilledRoundedRect(panelX + 12, rectY, panelWidth - 24, rowH - 4, 8,
                                                      Colors::Selected);
                    framebuffer.drawRoundedRect(panelX + 12, rectY, panelWidth - 24, rowH - 4, 8, Colors::Accent, 2);
                }
                const bool legal = reorder && row < screen.pickerLegalCount;
                const Color rowColor = legal ? Color(120, 210, 130) : (isSelected ? Colors::Text : Colors::TextDim);
                const std::string label = pickerRowLabel(screen, kind, value);
                framebuffer.drawText(panelX + 28, rectY + (rowH - 4 - framebuffer.lineHeight(TextStyle::Body)) / 2,
                                     label, rowColor);
                // id = option row
                screen.touchButtons.push_back({entryIndex, panelX + 12, rectY, panelWidth - 24, rowH - 4});
            }

            if (matchCount == 0)
            {
                framebuffer.drawText(panelX + 28, listTop + 8, "没有符合搜索条件的内容。", Colors::TextDim);
                int emptyStateY = listTop + 8 + framebuffer.lineHeight(TextStyle::Body) + 4;
                framebuffer.drawText(panelX + 28, emptyStateY, "Y：修改搜索，或清除搜索以查看整个列表。",
                            Colors::TextDim, TextStyle::Caption);
                emptyStateY += framebuffer.lineHeight(TextStyle::Caption) + 10;

                // The met list is IN-WORLD PLACES only. Who an egg came FROM -- "Nursery Couple",
                // "a treasure hunter", Riley -- is a different bank of ids and belongs to the Egg Loc
                // field, so searching for one here correctly finds nothing. Correctly is not the same
                // as usefully: the reporter who went looking for "Nursery Couple" got this blank panel
                // and no way to know the entry exists one row away. Say where it is.
                if (kind == PickerKind::MetLocation && !screen.pickerMetIsEgg)
                {
                    const Names::LocationTable eggSources = Names::getEggSourceLocationTable(screen.pickerMetVersion);
                    const char *matchedSource = nullptr;
                    for (uint16_t sourceIndex = 0; sourceIndex < eggSources.count && !matchedSource; ++sourceIndex)
                        if (eggSources.names[sourceIndex][0] != '\0' &&
                            screen.pickerSearch.matches(eggSources.names[sourceIndex]))
                            matchedSource = eggSources.names[sourceIndex];
                    if (matchedSource)
                    {
                        framebuffer.drawText(panelX + 28, emptyStateY,
                                    std::string("\"") + matchedSource + "\" is where an EGG came from, not a place.",
                                    Colors::Accent, TextStyle::Caption);
                        framebuffer.drawText(panelX + 28, emptyStateY + framebuffer.lineHeight(TextStyle::Caption) + 4,
                                    "请在蛋获得地点一栏中设置它。", Colors::Accent, TextStyle::Caption);
                    }
                }
            }

            // Scrollbar on the panel's right edge (same thumb as everywhere else) when the list overflows.
            drawScrollbar(framebuffer, panelX + panelWidth - 14, listTop, visible * rowH, matchCount * rowH,
                          first * rowH);

            // No hint strip in the card. The screen's nav bar is the ONE place controls are listed; a
            // second copy right above it is two rows of buttons for one dialog, saying different things.
            // TrainerViewScreen's instruction chain names this picker's keys while it is open, and
            // PokemonDetailsModal does the same when it is drawn over the details page.
        }
    }
}
