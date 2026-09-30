#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

#include "UI/Panels/StoragePanel.h"
#include "UI/Panels/CarriedSprite.h" // drawLiftedMon -- shared with the Boxes view
#include "UI/TrainerViewScreen.h"
#include "Trainer/OriginStamp.h" // originStampLabel -- the origin stamp on the detail strip
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "UI/Dialogs/EditControls.h" // drawEditChoiceButton -- dialog buttons, drawn and hit-tested together
#include "UI/ScreenChrome.h"         // on-button controller glyphs
#include "UI/Dialogs/DialogFrame.h"
#include "Names/FormNames.h" // getDisplayName -- variant prefix ("Alolan Raichu", etc.)
#include "Names/ItemNames.h" // getItemNameFor -- the item that chooses a trade evolution
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/Evolution.h" // getTradeEvolutionOffer -- the trade-evolve destination choice
#include "Save/TitleFolder.h" // localizedGameVersionName -- transfer warning title

using namespace Trainer;

namespace UI
{
    namespace Panels
    {

        namespace
        {
            Color cursorColorFor(TrainerViewScreen::CursorMode mode)
            {
                switch (mode)
                {
                case TrainerViewScreen::CursorMode::Move:
                    return Colors::CursorMove;
                case TrainerViewScreen::CursorMode::Multi:
                    return Colors::CursorMulti;
                case TrainerViewScreen::CursorMode::Menu:
                default:
                    return Colors::CursorMenu;
                }
            }

            const char *modeName(TrainerViewScreen::CursorMode mode)
            {
                switch (mode)
                {
                case TrainerViewScreen::CursorMode::Move:
                    return "移动";
                case TrainerViewScreen::CursorMode::Multi:
                    return "多选";
                case TrainerViewScreen::CursorMode::Menu:
                default:
                    return "菜单";
                }
            }

            // Cell geometry of a drawn pane, handed back so the carried block and the cursor can be
            // drawn AFTER both panes (otherwise the second pane's card paints over a block being
            // carried across the boundary).
            struct PaneGeom
            {
                int gridX = 0, gridTop = 0, colPitch = 0, rowPitch = 0, discR = 0, cols = 1, rows = 5;
                int pillCx = 0, pillTop = 0; // box-name pill, so the cursor can point at it too
                int cellX(int slot) const { return gridX + (slot % cols) * colPitch; }
                int cellY(int slot) const { return gridTop + (slot / cols) * rowPitch; }
                int centerX(int slot) const { return cellX(slot) + colPitch / 2; }
                int centerY(int slot) const { return cellY(slot) + rowPitch / 2; }
            };

            // Draw one Pokemon (sprite + shiny/party markers) centered on a slot disc of radius discR.
            void drawSlotDisc(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer,
                              const Pokemon::Pokemon *pokemon, bool savePane, int boxIndex, int slotIndex,
                              int centerX, int centerY, int discR)
            {
                if (!pokemon || pokemon->speciesID() == 0)
                    return;

                const std::string speciesName(pokemon->species());
                const bool isShiny = pokemon->isShiny(pokemon->id32(), speciesName);

                const int size = static_cast<int>(discR * 1.75);
                if (pokemon->isEgg())
                {
                    framebuffer.drawEgg(centerX, centerY, size); // eggs show as an egg in the grid
                }
                else
                {
                    Sprite *sprite = SpriteManager::getIconSprite(pokemon->speciesID(), pokemon->form(), isShiny);
                    if (sprite && sprite->data)
                        framebuffer.drawImageScaled(centerX - size / 2, centerY - size / 2, sprite->width,
                                                    sprite->height, size, size, sprite->data, sprite->channels);
                }

                if (isShiny)
                    // top-right
                    framebuffer.drawShinyMark(centerX + discR - 15, centerY - discR + 1, 15, Colors::ShinyStar);

                // Party-membership badge (save pane only): partner heart (top-left) or party number (bottom-left).
                if (savePane)
                {
                    if (screen.trainer.isStarterPokemon(boxIndex, slotIndex))
                    {
                        framebuffer.drawSymbol(centerX - discR, centerY - discR + 2, "\xE2\x99\xA5",
                                               Colors::PartnerHeart);
                    }
                    else
                    {
                        int partyPos = screen.trainer.getPartyPosition(boxIndex, slotIndex);
                        if (partyPos > 0)
                        {
                            // Gold badge + dark digit at the disc's bottom-left, legible on any sprite/theme.
                            const std::string count = std::to_string(partyPos);
                            const int boxX = centerX - discR + 9, by = centerY + discR - 9;
                            framebuffer.drawFilledCircle(boxX, by, 9, Colors::PartyBadge);
                            int textWidth, th;
                            framebuffer.measureText(count, textWidth, th, TextStyle::Caption);
                            framebuffer.drawText(boxX - textWidth / 2, by - th / 2, count, Colors::PartyBadgeText,
                                                 TextStyle::Caption);
                        }
                    }
                }
            }

            // Draws one storage pane in the HOME box style (rounded card + indigo header band + disc
            // grid), and reports its cell geometry via outGeom.
            void drawPane(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer,
                          int panelX, int panelY, int panelWidth, int panelHeight, bool savePane,
                          int boxIndex, int cursorSlot, bool focused, bool entered,
                          int cols, int rows, int slotsPerBox,
                          const std::string &label, int boxCount, PaneGeom &outGeom)
            {
                framebuffer.drawFilledRoundedRect(panelX, panelY, panelWidth, panelHeight, 16, Colors::Panel);

                // Header band (rounded top). Focused pane = accent indigo; unfocused = dim.
                constexpr int headerH = 44;
                const Color band = focused ? Colors::AccentDim : Colors::Border;
                framebuffer.drawFilledRoundedRect(panelX, panelY, panelWidth, headerH, 16, band);
                framebuffer.drawFilledRect(panelX, panelY + headerH - 16, panelWidth, 16, band);

                int labelWidth, lh;
                framebuffer.measureText(label, labelWidth, lh, TextStyle::Body);
                const int pillW = std::min(panelWidth - 120, labelWidth + 40), pillH = 28;
                const int pillX = panelX + (panelWidth - pillW) / 2, pillY = panelY + (headerH - pillH) / 2;
                // Amber pill when the header itself is focused (navigate up to it, or tap it, to rename
                // this box). cursorSlot == -1 is the "header focused" sentinel.
                const bool headerFocused = focused && entered && cursorSlot == -1;
                framebuffer.drawPill(pillX, pillY, pillW, pillH, headerFocused ? Colors::Primary : Colors::Panel);
                framebuffer.drawText(panelX + (panelWidth - labelWidth) / 2, pillY + (pillH - lh) / 2, label,
                                     headerFocused ? Colors::PrimaryText : Colors::Text);

                const int arrowY = panelY + (headerH - lh) / 2;
                // left
                framebuffer.drawSymbol(panelX + 18, arrowY, "\xE2\x97\x80", focused ? Colors::Text : Colors::TextDim);
                framebuffer.drawSymbol(panelX + panelWidth - 32, arrowY, "\xE2\x96\xB6",
                                       focused ? Colors::Text : Colors::TextDim); // right
                // Tappable box arrows (special slot ids -2 = previous box, -3 = next box) and the name pill
                // (-4 = rename this box). The name target sits between the two arrow zones.
                screen.storageTouchTargets.push_back({savePane ? 0 : 1, boxIndex, -2, panelX, panelY, 64, headerH});
                screen.storageTouchTargets.push_back(
                    {savePane ? 0 : 1, boxIndex, -3, panelX + panelWidth - 64, panelY, 64, headerH});
                screen.storageTouchTargets.push_back(
                    {savePane ? 0 : 1, boxIndex, -4, panelX + 64, panelY, panelWidth - 128, headerH});
                std::string counter = std::to_string(boxIndex + 1) + " / " + std::to_string(boxCount);
                int captionWidth, ch;
                framebuffer.measureText(counter, captionWidth, ch, TextStyle::Caption);
                framebuffer.drawText(panelX + panelWidth - 44 - captionWidth, panelY + (headerH - ch) / 2, counter,
                                     focused ? Colors::Text : Colors::TextDim, TextStyle::Caption);

                const int gridTop = panelY + headerH + 8;
                const int gridW = panelWidth - 28, gridH = panelHeight - headerH - 18;
                // Both panes use the same cell pitch -- the reference width -- so a narrow save box
                // (Gen 1: 20 slots, 4 wide) has cells identical to the bank's beside it instead of
                // fatter ones, and its rows line up with the bank's row for row. The narrow grid is
                // centred in its pane; shifting the ORIGIN means cellX(), the multi-select rect, the
                // drag preview and the touch targets all move with it and cannot disagree.
                const int colPitch = gridW / BOX_GRID_REFERENCE_COLUMNS, rowPitch = gridH / rows;
                const int gridX = panelX + 14 + boxGridCenterOffset(cols, colPitch);
                int discR = std::min(colPitch, rowPitch) / 2 - 4;
                if (discR < 12)
                    discR = 12;
                outGeom = PaneGeom{gridX, gridTop, colPitch, rowPitch, discR, cols, rows,
                                   panelX + panelWidth / 2, pillY};

                const int thisPane = savePane ? 0 : 1;

                // The rubber-band rectangle being swept out in Multi mode: a green wash UNDER the
                // Pokemon (so they stay readable) with a matching border on top. It spans from the
                // anchor cell to the cell under the cursor, the way HOME's multi-select highlight does.
                int selC0 = -1, selR0 = -1, selC1 = -1, selR1 = -1;
                if (screen.currentlySelecting && entered && screen.selectPane == thisPane &&
                    screen.selectBox == boxIndex && cursorSlot >= 0)
                {
                    const int anchorColumn = screen.selectDimensions.first, ay = screen.selectDimensions.second;
                    const int cursorColumn = cursorSlot % cols, cys = cursorSlot / cols;
                    selC0 = std::min(anchorColumn, cursorColumn);
                    selC1 = std::max(anchorColumn, cursorColumn);
                    selR0 = std::min(ay, cys);
                    selR1 = std::max(ay, cys);
                    const Color wash(Colors::CursorMulti.red, Colors::CursorMulti.green, Colors::CursorMulti.blue, 64);
                    framebuffer.drawFilledRoundedRect(gridX + selC0 * colPitch + 2, gridTop + selR0 * rowPitch + 2,
                                             (selC1 - selC0 + 1) * colPitch - 4, (selR1 - selR0 + 1) * rowPitch - 4,
                                             14, wash);
                }

                for (int slotIndex = 0; slotIndex < slotsPerBox; ++slotIndex)
                {
                    const int row = slotIndex / cols, col = slotIndex % cols;
                    const int cellX = gridX + col * colPitch, cellY = gridTop + row * rowPitch;
                    const int centerX = cellX + colPitch / 2, cy = cellY + rowPitch / 2;

                    // Whole-cell touch target (consumed next frame in update()).
                    screen.storageTouchTargets.push_back(
                        {thisPane, boxIndex, slotIndex, cellX, cellY, colPitch, rowPitch});

                    const Pokemon::Pokemon *pokemon =
                        savePane ? screen.trainer.boxes[boxIndex][slotIndex].get()
                                 : (screen.bank ? screen.bank->boxes[boxIndex][slotIndex].get() : nullptr);
                    const bool empty = !pokemon || pokemon->speciesID() == 0;

                    framebuffer.drawFilledCircle(centerX, cy, discR, empty ? Colors::Panel : Colors::PanelAlt);
                    if (empty)
                        framebuffer.drawCircle(centerX, cy, discR, Colors::Border, 1);

                    drawSlotDisc(screen, framebuffer, pokemon, savePane, boxIndex, slotIndex, centerX, cy, discR);

                    // Box/Bank search: a box is a grid the player arranged, so matches
                    // are RINGED and the rest dimmed rather than filtered out -- hiding slots would
                    // move everything else, and this storage is positional.
                    if (screen.storageSearch.isFiltering())
                    {
                        if (screen.storageSlotMatchesSearch(thisPane, boxIndex, slotIndex))
                            framebuffer.drawCircle(centerX, cy, discR + 2, Colors::Accent, 3);
                        else
                            framebuffer.drawFilledCircle(centerX, cy, discR, Color(0, 0, 0, 140));
                    }
                }

                if (selC0 >= 0)
                {
                    framebuffer.drawRoundedRect(gridX + selC0 * colPitch + 2, gridTop + selR0 * rowPitch + 2,
                                       (selC1 - selC0 + 1) * colPitch - 4, (selR1 - selR0 + 1) * rowPitch - 4,
                                       14, Colors::CursorMulti, 3);
                }
            }

            // The carried block plus the pointer cursor, drawn on top of BOTH panes so a group stays
            // fully visible while it travels across the screen and over the pane boundary.
            void drawCarryAndCursor(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer, const PaneGeom &g,
                                    int cursorSlot)
            {
                const Color cursorColor = cursorColorFor(screen.cursorMode);
                // Gentle vertical bob, a HOME-style pointer wobble done as a sine so it is framerate-independent.
                const int bobOffset = static_cast<int>(std::sin(framebuffer.getTimeSeconds() * 3.4) * 3.0);
                // The head is symmetric about its point, so the point goes straight on the slot's
                // centre line -- no horizontal nudge needed.

                // The box-name pill is a cursor position of its own (slot -1): navigate up off the top
                // row onto it and A renames the box. Point at it with the same arrow so the cursor is
                // never invisible, rather than relying on the pill's amber highlight alone.
                if (cursorSlot < 0)
                {
                    framebuffer.drawPointerCursor(g.pillCx, g.pillTop - 2 + bobOffset, GRID_CURSOR_HEIGHT, cursorColor);
                    return; // the header is unreachable while carrying
                }

                if (screen.carrying())
                {
                    const int selectionColumns = std::max(1, screen.selectDimensions.first);
                    const int selectionRows = std::max(1, screen.selectDimensions.second);
                    const bool fits = screen.checkPutDownBounds();
                    // A group that would hang off the grid is tinted with the warning colour, so the
                    // "must land in the exact slots" rule is visible before you press A rather than after.
                    const Color tint = fits ? cursorColor : Colors::Warning;
                    const int baseX = g.cellX(cursorSlot), baseY = g.cellY(cursorSlot);
                    for (int yIndex = 0; yIndex < selectionRows; ++yIndex)
                    {
                        for (int columnOffset = 0; columnOffset < selectionColumns; ++columnOffset)
                        {
                            const size_t index = static_cast<size_t>(yIndex) * static_cast<size_t>(selectionColumns) +
                                                 static_cast<size_t>(columnOffset);
                            if (index >= screen.moveMon.size())
                                continue;
                            const int cellCenterX = baseX + columnOffset * g.colPitch + g.colPitch / 2;
                            const int cellCenterY = baseY + yIndex * g.rowPitch + g.rowPitch / 2 + bobOffset;
                            // Backing tile marks the footprint the block will occupy -- including its
                            // holes, which stay empty when it lands.
                            if (selectionColumns > 1 || selectionRows > 1)
                                framebuffer.drawFilledRoundedRect(
                                    cellCenterX - g.colPitch / 2 + 3, cellCenterY - g.rowPitch / 2 + 3, g.colPitch - 6,
                                    g.rowPitch - 6, 14, Color(tint.red, tint.green, tint.blue, 70));
                            drawLiftedMon(framebuffer, screen.moveMon[index].get(), cellCenterX, cellCenterY, g.discR);
                        }
                    }
                    if (selectionColumns > 1 || selectionRows > 1)
                        framebuffer.drawRoundedRect(baseX + 3, baseY + 3 + bobOffset, selectionColumns * g.colPitch - 6,
                                                    selectionRows * g.rowPitch - 6, 14, tint, 3);
                }

                // The cursor itself, in the active mode's colour: its point rests on the top of the
                // slot's disc rather than in the middle of it, so the Pokemon, its shiny mark and its
                // party badge all stay visible. On the top row the body runs up over the box-name pill
                // -- deliberately, the way HOME's cursor does; it is drawn last, so it floats over the
                // header instead of being clipped by it.
                const int cellCenterX = g.centerX(cursorSlot), cellCenterY = g.centerY(cursorSlot);
                framebuffer.drawPointerCursor(cellCenterX, cellCenterY - g.discR - 3 + bobOffset, GRID_CURSOR_HEIGHT,
                                              cursorColor);
            }
        }

        void drawStorageView(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer, int storageViewX,
                             int storageViewY, int storageViewWidth, int storageViewHeight)
        {
            if (!screen.bank)
            {
                framebuffer.drawCard(storageViewX, storageViewY, storageViewWidth, storageViewHeight);
                framebuffer.drawText(storageViewX + 16, storageViewY + 70, "银行不可用", Colors::TextDim);
                return;
            }

            const bool entered = screen.detailViewActive; // cursor only shows once the view is entered
            screen.storageTouchTargets.clear();           // rebuilt each frame by drawPane below

            constexpr int paneGap = 16;
            constexpr int infoH = 60;
            const int paneW = (storageViewWidth - paneGap) / 2;
            const int paneH = storageViewHeight - infoH - 8;

            const int saveSlots = static_cast<int>(screen.trainer.getSlotsPerBox());
            const int saveCols = boxGridColumns(saveSlots);
            const int bankSlots = static_cast<int>(Bank::BANK_SLOTS_PER_BOX);
            const int bankCols = boxGridColumns(bankSlots);
            const int rows = BOX_GRID_ROWS;

            const bool saveFocused = screen.storageFocusPane == 0;
            const bool bankFocused = screen.storageFocusPane == 1;

            std::string saveLabel = screen.selectedBoxIndex < static_cast<int>(screen.trainer.boxNames.size())
                                        ? screen.trainer.boxNames[screen.selectedBoxIndex]
                                        : ("盒子 " + std::to_string(screen.selectedBoxIndex + 1));
            std::string bankLabel = screen.bank->boxDisplayName(screen.stBankBox);

            PaneGeom saveGeom, bankGeom;

            drawPane(screen, framebuffer, storageViewX, storageViewY, paneW, paneH, /*savePane*/ true,
                     screen.selectedBoxIndex, screen.stSaveSlot, saveFocused, entered,
                     saveCols, rows, saveSlots, saveLabel, static_cast<int>(screen.trainer.getBoxCount()), saveGeom);

            drawPane(screen, framebuffer, storageViewX + paneW + paneGap, storageViewY, paneW, paneH,
                     /*savePane*/ false, screen.stBankBox, screen.stBankSlot, bankFocused, entered, bankCols, rows,
                     bankSlots, bankLabel, static_cast<int>(Bank::BANK_BOX_COUNT), bankGeom);

            // Cursor + carried block last, over BOTH panes: a group being carried from one side to the
            // other must not be painted over by the destination pane's card.
            if (entered)
            {
                drawCarryAndCursor(screen, framebuffer, saveFocused ? saveGeom : bankGeom,
                                   saveFocused ? screen.stSaveSlot : screen.stBankSlot);
            }

            const int infoRowY = storageViewY + paneH + 8;
            framebuffer.drawCard(storageViewX, infoRowY, storageViewWidth, infoH);

            if (!entered)
            {
                framebuffer.drawText(storageViewX + 16, infoRowY + infoH / 2 - 11, "按 A 打开存储界面",
                                     Colors::TextDim, TextStyle::Body);
                return;
            }

            // Mode swatch (dot) + name.
            const Color modeCol = cursorColorFor(screen.cursorMode);
            framebuffer.drawFilledCircle(storageViewX + 22, infoRowY + infoH / 2, 8, modeCol);
            framebuffer.drawText(storageViewX + 38, infoRowY + infoH / 2 - 11, modeName(screen.cursorMode),
                                 Colors::Text, TextStyle::Body);

            // Right side: how big the group in hand (or being swept out) is, in the mode colour.
            std::string selectionText;
            if (screen.carrying() && screen.carriedCount() > 1)
            {
                selectionText = std::to_string(screen.carriedCount()) + " 只拿起中（" +
                                std::to_string(screen.selectDimensions.first) + "×" +
                                std::to_string(screen.selectDimensions.second) + "）";
            }
            else if (screen.currentlySelecting)
            {
                selectionText = "选择中 - 按 A 拿起整组";
            }
            if (!selectionText.empty())
            {
                int selectionTextWidth, sh;
                framebuffer.measureText(selectionText, selectionTextWidth, sh);
                framebuffer.drawText(storageViewX + storageViewWidth - 16 - selectionTextWidth,
                                     infoRowY + infoH / 2 - 11, selectionText, Colors::CursorMulti, TextStyle::Body);
            }

            // Middle: the held Pokemon, else the one under the focused cursor. When the box-name header
            // is focused (slot == -1) there is no cursor pokemon, so prompt the rename instead of indexing.
            const int focusSlot = saveFocused ? screen.stSaveSlot : screen.stBankSlot;
            const bool holding = screen.carrying();
            const Pokemon::Pokemon *focus = screen.firstCarried();
            if (!focus && focusSlot >= 0)
            {
                focus = saveFocused ? screen.trainer.boxes[screen.selectedBoxIndex][focusSlot].get()
                                    : screen.bank->boxes[screen.stBankBox][focusSlot].get();
            }

            const int labelX = storageViewX + 120;
            if (focusSlot == -1 && !holding)
            {
                framebuffer.drawText(labelX, infoRowY + infoH / 2 - 11, "已选择盒子名称 - 按 A 重命名",
                                     Colors::TextDim, TextStyle::Body);
            }
            else if (holding && screen.carriedCount() > 1)
            {
                // A whole group in hand: name the leader and say how many ride with it, rather than
                // pretending one Pokemon is the whole payload.
                std::string display =
                    Names::getDisplayName(focus->speciesID(), focus->form(), std::string(focus->species()));
                std::string line = "拿起中：" + display + " +" + std::to_string(screen.carriedCount() - 1) +
                                   (screen.checkPutDownBounds() ? "   按 A 放在此处" : "   此处空间不足");
                framebuffer.drawText(labelX, infoRowY + infoH / 2 - 11, line,
                            screen.checkPutDownBounds() ? Colors::Text : Colors::Warning, TextStyle::Body);
            }
            else if (focus && focus->speciesID() != 0)
            {
                std::string name(focus->species());
                const bool shiny = focus->isShiny(focus->id32(), name);
                std::string display = Names::getDisplayName(focus->speciesID(), focus->form(), name);
                // Level then dex number, matching the box Summary panel and the details page --
                // the same pokemon should not be spelled two ways in two views of one screen.
                std::string line = (holding ? "拿起中：" : "") + display +
                                   "   等级 " + std::to_string(focus->level()) +
                                   "   No. " + dexNumberLabel(focus->speciesID());
                framebuffer.drawText(labelX, infoRowY + infoH / 2 - 11, line, Colors::Text, TextStyle::Body);
                int labelWidth, lh;
                framebuffer.measureText(line, labelWidth, lh, TextStyle::Body);
                int markX = labelX + labelWidth + 10;
                const char *g = focus->genderSymbol();
                if (g[0] != '\0')
                {
                    framebuffer.drawSymbol(markX, infoRowY + infoH / 2 - 11, g,
                                           (std::string(g) == "\xE2\x99\x82") ? Colors::Blue : Colors::Magenta);
                    markX += 20;
                }
                if (shiny)
                {
                    framebuffer.drawShinyMark(markX, infoRowY + infoH / 2 - 11, 16, Colors::ShinyStar);
                    markX += 22;
                }
                // ORIGIN STAMP, trailing the gender/shiny marks so those stay adjacent to the name.
                // Suppressed while the right-hand side is reporting a selection, which is the more
                // urgent state and is right-aligned into the same strip.
                //
                // THE OPEN SAVE IS THE TITLE CONTEXT ONLY FOR A POKEMON SITTING IN THE SAVE'S OWN
                // BOXES. A bank record has none -- the bank's tag records a locale, not a title --
                // and one in hand came from whichever pane it was lifted out of, which the focused
                // pane no longer tells us. Both pass nullptr and stop at the generation.
                if (selectionText.empty())
                {
                    const Trainer::Trainer *originSave =
                        (!holding && saveFocused) ? &screen.trainer : nullptr;
                    markX += 12;
                    Sprite *originMark = SpriteManager::getOriginMarkSprite(
                        Trainer::originMarkFor(*focus), Colors::TextDim);
                    if (originMark != nullptr && originMark->data != nullptr)
                    {
                        const int markSize = 18;
                        framebuffer.drawImageScaled(markX, infoRowY + infoH / 2 - 10, originMark->width,
                                                    originMark->height, markSize, markSize,
                                                    originMark->data, originMark->channels);
                        markX += markSize + 6;
                    }
                    // Gen 1/2 have no name worth printing -- the mark IS the answer. Gen 3/4/5 have
                    // no mark, so the name is the answer. Gen 6+ get both.
                    if (focus->hasOriginGame())
                    {
                        framebuffer.drawText(markX, infoRowY + infoH / 2 - 11,
                                             Trainer::originStampLabel(*focus, originSave),
                                             Colors::TextDim, TextStyle::Body);
                    }
                }
            }
            else
            {
                framebuffer.drawText(labelX, infoRowY + infoH / 2 - 11, "空槽位", Colors::TextDim, TextStyle::Body);
            }
        }

        namespace
        {
            // A small centered popup menu (scrim + card + title + item list). Each row is registered as
            // a touch button (id = item index) so a tap selects + confirms it.
            void drawPopupMenu(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer, const std::string &title,
                               const char *const *items, int count, int selectedIndex, uint32_t disabledMask = 0)
            {
                // Touch-friendly sizing: 56px rows (TouchTargetMin) and a wide card so rows are easy to tap.
                constexpr int dialogWidth = 440, rowH = TouchTargetMin;
                constexpr int headerH = 60;
                const int dialogHeight = headerH + count * rowH + 14;
                const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
                const int dialogY = (framebuffer.getHeight() - dialogHeight) / 2;
                constexpr int cornerRadius = Dialogs::DIALOG_CORNER_RADIUS; // same card shape as every other modal
                framebuffer.drawFilledRect(0, 0, framebuffer.getWidth(), framebuffer.getHeight(), Color(0, 0, 0, 130));
                framebuffer.drawSoftShadow(dialogX, dialogY, dialogWidth, dialogHeight, cornerRadius);
                framebuffer.drawFilledRoundedRect(dialogX, dialogY, dialogWidth, dialogHeight, cornerRadius,
                                                  Colors::Panel);
                framebuffer.drawRoundedRect(dialogX, dialogY, dialogWidth, dialogHeight, cornerRadius, Colors::Border,
                                            1);
                framebuffer.drawText(dialogX + 22, dialogY + 16, title, Colors::Text, TextStyle::Heading);
                framebuffer.drawFilledRect(dialogX + cornerRadius, dialogY + headerH - 4,
                                           dialogWidth - cornerRadius * 2, 2, Colors::Accent);
                screen.touchButtons.clear();
                int rectY = dialogY + headerH;
                for (int index = 0; index < count; ++index)
                {
                    const bool disabled = (disabledMask >> index) & 1u;
                    const bool selRow = (index == selectedIndex) && !disabled;
                    if (selRow)
                        framebuffer.drawSelectionHighlight(dialogX + 10, rectY + 3, dialogWidth - 20, rowH - 6);
                    int textWidth, th;
                    framebuffer.measureText(items[index], textWidth, th, TextStyle::Body);
                    const Color rowColor = disabled ? Colors::Border : (selRow ? Colors::Text : Colors::TextDim);
                    framebuffer.drawText(dialogX + 28, rectY + (rowH - th) / 2, items[index], rowColor,
                                         TextStyle::Body);
                    if (!disabled)
                        // greyed rows aren't tappable
                        screen.touchButtons.push_back({index, dialogX + 10, rectY, dialogWidth - 20, rowH});
                    rectY += rowH;
                }
            }
        }

        void drawStorageActionMenu(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const Pokemon::Pokemon *pokemon =
                screen.storageSlot(screen.menuPane, screen.menuBox, screen.menuSlot).get();
            const std::string title =
                (pokemon && pokemon->speciesID() != 0)
                    ? Names::getDisplayName(pokemon->speciesID(), pokemon->form(), pokemon->species())
                    : "宝可梦";
            static const char *const items[] = {"招式", "编辑", "复制", "放生", "Find\u2026", "取消"};
            // A party-linked (LGPE) slot can be edited or cloned, but not moved or released -- grey those
            // two out. (Same lock rule as storageSlotLocked: save pane + a party member points here.)
            const bool locked = screen.menuPane == 0 &&
                                screen.trainer.getPartyPosition(screen.menuBox, screen.menuSlot) > 0;
            const uint32_t disabled = locked ? ((1u << 0) | (1u << 3)) : 0u; // Move (0), Release (3)
            drawPopupMenu(screen, framebuffer, title, items, 6, screen.storageMenuIndex, disabled);
        }

        // Options for the block in hand. There is deliberately no "move" entry: a carried group is moved
        // by carrying it to the destination and pressing A, which is what makes placement positional.
        void drawStorageGroupMenu(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const int count = screen.carriedCount();
            const std::string title = std::to_string(count) + (count == 1 ? " 只宝可梦拿起中" : " 只宝可梦拿起中");
            static const char *const items[] = {"全部放生", "放回原来的位置", "取消"};
            drawPopupMenu(screen, framebuffer, title, items, 3, screen.groupMenuIndex);
        }

        void drawStorageReleaseConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            std::string message;
            if (screen.releaseGroup)
            {
                message = "放生" + std::to_string(screen.carriedCount()) + "只宝可梦？";
            }
            else
            {
                const Pokemon::Pokemon *pokemon =
                    screen.storageSlot(screen.releasePane, screen.releaseBox, screen.releaseSlot).get();
                const std::string subjectName =
                    (pokemon && pokemon->speciesID() != 0)
                        ? Names::getDisplayName(pokemon->speciesID(), pokemon->form(), pokemon->species())
                        : "这只宝可梦";
                message = "放生" + subjectName + "?";
            }
            constexpr int dialogWidth = 540, h = 226;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - h) / 2;
            int centerY =
                Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth, h, "放生", Colors::Red);
            framebuffer.drawText(dialogX + 28, centerY, message, Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 34, "此操作无法撤销。", Colors::TextDim,
                                 TextStyle::Caption);

            // Buttons carry their glyph (B: Cancel, A: Release); Release stays red.
            screen.touchButtons.clear();
            const int boxWidth = 190, bh = TouchTargetMin, by = dialogY + h - bh - 18;
            const int relX = dialogX + dialogWidth - boxWidth - 20;   // right = Release (id 1 -> A)
            const int cancelX = relX - boxWidth - 16; // left  = Cancel  (id 0 -> B)
            Dialogs::drawEditChoiceButton(screen, framebuffer, cancelX, by, boxWidth, bh, "B", "取消", 0,
                                          Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, relX, by, boxWidth, bh, "A", "放生", 1, Colors::Red,
                                          Colors::White);
        }

        // Creator "保留这只新宝可梦吗？" confirm. Three glyph buttons (like the Edit Item dialog),
        // so each is one controller button rather than a Left/Right selector: B = Back (return to
        // editing), Y = Discard (remove it), A = Keep. ids 0 / 2 / 1 respectively.
        void drawCreatorKeepConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            constexpr int dialogWidth = 540, h = 226;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - h) / 2;
            int centerY =
                Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth, h, "新建宝可梦", Colors::Primary);
            framebuffer.drawText(dialogX + 28, centerY, "保留这只新宝可梦吗？", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 34, "放弃将移除它；返回可继续编辑。", Colors::TextDim,
                                 TextStyle::Caption);

            screen.touchButtons.clear();
            const int choiceButtonWidth = 160, cbh = TouchTargetMin, cby = dialogY + h - cbh - 18;
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + 24, cby, choiceButtonWidth, cbh, "B", "返回",
                                          0, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + (dialogWidth - choiceButtonWidth) / 2, cby,
                                          choiceButtonWidth, cbh, "Y", "放弃", 2, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + dialogWidth - 24 - choiceButtonWidth, cby,
                                          choiceButtonWidth, cbh, "A", "保留", 1, Colors::PanelAlt);
        }

        void drawDetailsDiscardConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            // Deliberately the same shape as the creator's Keep/Discard above: same three glyph
            // buttons in the same places, so leaving an edit page always looks and answers alike.
            // What differs is the stakes -- here the pokemon already exists and only the EDITS are at
            // risk, so the safe action (Save) sits on A where Keep sits for a new pokemon.
            constexpr int dialogWidth = 560, h = 226;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - h) / 2;
            int centerY = Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth, h, "未保存的更改",
                                                   Colors::Orange);
            framebuffer.drawText(dialogX + 28, centerY, "这只宝可梦有尚未保存的更改。", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 34, "放弃将丢失这些更改；返回可继续编辑。",
                        Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.clear();
            const int choiceButtonWidth = 160, cbh = TouchTargetMin, cby = dialogY + h - cbh - 18;
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + 24, cby, choiceButtonWidth, cbh, "B", "返回",
                                          0, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + (dialogWidth - choiceButtonWidth) / 2, cby,
                                          choiceButtonWidth, cbh, "Y", "放弃", 2, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + dialogWidth - 24 - choiceButtonWidth, cby,
                                          choiceButtonWidth, cbh, "A", "保存", 1, Colors::PanelAlt);
        }

        void drawTradeEvolveChoice(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            // WHICH POKEMON IT BECOMES. The held item is what the games use to choose, and it is
            // not a condition on evolving here -- so when it names nothing and there is more than
            // one destination, the only thing left to ask is the user. That is Clamperl holding
            // neither a Deep Sea Tooth nor a Deep Sea Scale, and nothing else in any generation.
            //
            // A LIST, not a pair of lettered buttons: it is an element picker like the donor title
            // picker beside it, and a list stays right if a later game ever adds a third row.
            const Pokemon::Pokemon *pokemon = screen.detailsTargetPokemon();
            if (!pokemon)
                return;
            const Pokemon::TradeEvolutionOffer offer = Pokemon::getTradeEvolutionOffer(*pokemon);
            if (offer.candidateCount <= 0)
                return;

            constexpr int rowHeight = 46;
            const int dialogWidth = 620, dialogHeight = 186 + offer.candidateCount * rowHeight;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - dialogHeight) / 2;
            const int contentY = Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth,
                                                          dialogHeight, "交换进化", Colors::Accent);
            framebuffer.drawText(dialogX + 28, contentY, "它有两种交换进化结果，且未携带任何相关道具。",
                                 Colors::Text);
            framebuffer.drawText(dialogX + 28, contentY + 30, "选择它要进化成哪一种。",
                                 Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.clear();
            for (int candidateIndex = 0; candidateIndex < offer.candidateCount; ++candidateIndex)
            {
                const Pokemon::TradeEvolution &becomes = offer.candidates[candidateIndex];
                const int rowY = contentY + 64 + candidateIndex * rowHeight;
                const bool isSelected = (candidateIndex == screen.tradeEvolveChoiceIndex);
                if (isSelected)
                {
                    framebuffer.drawFilledRoundedRect(dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6, 8,
                                                      Colors::Selected);
                    framebuffer.drawRoundedRect(dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6, 8, Colors::Accent,
                                                2);
                }
                framebuffer.drawText(dialogX + 34, rowY + 12,
                                     Names::getDisplayName(becomes.destinationSpeciesId, becomes.destinationFormId,
                                                           Trainer::getSpeciesName(becomes.destinationSpeciesId)),
                                     isSelected ? Colors::Text : Colors::TextDim);
                // THE ITEM THAT WOULD HAVE CHOSEN IT, named in the pokemon's own game -- Deep Sea
                // Tooth is a different id in Gen 3 than it is from Gen 4 on. It says why these two
                // rows exist, which the destination names alone do not.
                const std::string chosenBy =
                    std::string("携带") + Names::getItemNameFor(pokemon->getGameGroup(), becomes.requiredItemId);
                int chosenByWidth = 0, chosenByHeight = 0;
                framebuffer.measureText(chosenBy, chosenByWidth, chosenByHeight, TextStyle::Caption);
                framebuffer.drawText(dialogX + dialogWidth - 34 - chosenByWidth, rowY + 15, chosenBy,
                                     Colors::TextDim, TextStyle::Caption);
                screen.touchButtons.push_back({candidateIndex, dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6});
            }
        }

        void drawTradeEvolveConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            // Same three-button shape as the Save/Discard prompt above, so every choice on this
            // page answers alike. A TRADE EVOLUTION FROM GEN 6 ON LEAVES A HANDLING TRAINER, and
            // that residue is the only thing that says the trade happened -- so the choice is not
            // cosmetic, and the dialog says which option is real and which is invented rather than
            // leaving the user to guess.
            constexpr int dialogWidth = 620, dialogHeight = 244;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - dialogHeight) / 2;
            const int centerY = Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth,
                                                         dialogHeight, "交换进化", Colors::Accent);
            framebuffer.drawText(dialogX + 28, centerY, "这只宝可梦会在交换时进化。", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 32,
                                 "Its game records the other trainer, so PKSE needs one.",
                                 Colors::TextDim, TextStyle::Caption);
            framebuffer.drawText(dialogX + 28, centerY + 58,
                                 "A save you own is a real trainer. Generated invents one.",
                                 Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.clear();
            const int choiceButtonWidth = 176, buttonHeight = TouchTargetMin;
            const int buttonY = dialogY + dialogHeight - buttonHeight - 18;
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + 24, buttonY, choiceButtonWidth,
                                          buttonHeight, "B", "取消", 0, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + (dialogWidth - choiceButtonWidth) / 2, buttonY,
                                          choiceButtonWidth, buttonHeight, "Y", "生成", 2, Colors::PanelAlt);
            Dialogs::drawEditChoiceButton(screen, framebuffer, dialogX + dialogWidth - 24 - choiceButtonWidth, buttonY,
                                          choiceButtonWidth, buttonHeight, "A", "使用存档", 1, Colors::PanelAlt);
        }

        void drawTradePartnerTitlePicker(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            // A SHORT LIST, NOT THE SAVE PICKER'S GRID. The grid is a boot step choosing what to
            // open; this is choosing one element out of a filtered handful, so it is a list in a
            // dialog like every other element picker on this page.
            const auto &candidates = screen.tradePartnerTitles.candidates;
            constexpr int rowHeight = 46, maxVisibleRows = 6;
            const int visibleRows = candidates.empty() ? 1
                                    : (static_cast<int>(candidates.size()) < maxVisibleRows
                                           ? static_cast<int>(candidates.size())
                                           : maxVisibleRows);
            const int dialogWidth = 620, dialogHeight = 150 + visibleRows * rowHeight;
            const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
            const int dialogY = (framebuffer.getHeight() - dialogHeight) / 2;
            const int contentY = Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth,
                                                          dialogHeight, "交换对象", Colors::Accent);

            screen.touchButtons.clear();
            if (candidates.empty())
            {
                // An empty list is a real answer -- no other save on this console can trade with
                // this Pokemon -- and saying so beats an empty panel, which reads as a failure.
                framebuffer.drawText(dialogX + 28, contentY + 8,
                                     "这台主机上没有其他存档可以与它交换。", Colors::TextDim);
                return;
            }

            const int scroll = screen.tradePartnerTitles.scroll;
            for (int visibleIndex = 0; visibleIndex < visibleRows; ++visibleIndex)
            {
                const int candidateIndex = scroll + visibleIndex;
                if (candidateIndex >= static_cast<int>(candidates.size()))
                    break;
                const TrainerViewScreen::ConsoleSaveEntry &entry = candidates[candidateIndex];
                const int rowY = contentY + visibleIndex * rowHeight;
                const bool isSelected = (candidateIndex == screen.tradePartnerTitles.selectedIndex);
                if (isSelected)
                {
                    framebuffer.drawFilledRoundedRect(dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6, 8,
                                                      Colors::Selected);
                    framebuffer.drawRoundedRect(dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6, 8, Colors::Accent,
                                                2);
                }
                framebuffer.drawText(dialogX + 34, rowY + 12, entry.label,
                                     isSelected ? Colors::Text : Colors::TextDim);
                // WHOSE SAVE IT IS, always. Two users on one console may own the same title, and
                // without the name those rows are the same string twice.
                int userNameWidth = 0, userNameHeight = 0;
                framebuffer.measureText(entry.userName, userNameWidth, userNameHeight, TextStyle::Caption);
                framebuffer.drawText(dialogX + dialogWidth - 34 - userNameWidth, rowY + 15, entry.userName,
                                     Colors::TextDim, TextStyle::Caption);
                screen.touchButtons.push_back({candidateIndex, dialogX + 20, rowY, dialogWidth - 40, rowHeight - 6});
            }
        }

        namespace
        {
            // Frame + Cancel/Continue buttons shared by the two lossy-move notices. Only the chrome is
            // shared; each dialog writes its own copy, because they warn about different losses.
            // Returns the content Y to start drawing body text at.
            int beginMoveConfirm(PKSEFramebuffer &framebuffer, const char *title, Color accent, int &dialogX,
                                 int &dialogY, int dialogWidth, int dialogHeight)
            {
                dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
                dialogY = (framebuffer.getHeight() - dialogHeight) / 2;
                return Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth, dialogHeight, title,
                                                accent);
            }

            void endMoveConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer, int dialogX, int dialogY,
                                int dialogWidth, int dialogHeight)
            {
                // On-button glyphs (B: Cancel, A: Continue) -- a single button each, no Left/Right selector.
                screen.touchButtons.clear();
                const int boxWidth = 190, bh = TouchTargetMin, by = dialogY + dialogHeight - bh - 18;
                const int contX = dialogX + dialogWidth - boxWidth - 20;   // right = Continue (id 1 -> A)
                const int cancelX = contX - boxWidth - 16; // left  = Cancel   (id 0 -> B)
                Dialogs::drawEditChoiceButton(screen, framebuffer, cancelX, by, boxWidth, bh, "B", "取消", 0,
                                              Colors::PanelAlt);
                Dialogs::drawEditChoiceButton(screen, framebuffer, contX, by, boxWidth, bh, "A", "继续", 1,
                                              Colors::PanelAlt);
            }
        }

        // Gen 3 (FireRed/LeafGreen or Ruby/Sapphire/Emerald). Down-converting rebuilds the PID to keep
        // the nature (Gen 3 has no nature field), which is destructive and cannot be undone -- so this
        // supersedes the other two notices when a move matches more than one. Like them it is shown
        // only while the Move warning setting is on.
        //
        // It also carries the ONE loss Gen 3 cannot reconstruct. A nickname has the species name to
        // fall back on, but a trainer's name is theirs, and Gen 3's character tables have no Hangul
        // and no CJK -- so a Korean or Chinese OT cannot be written there and the field arrives
        // blank. That is said HERE rather than in a notice of its own, because this setting gates
        // every bank transfer warning and a second dialog outside it could not be turned off.
        void drawGen3ConvertConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const bool originalTrainerNameDropped = screen.pendingMoveDropsOriginalTrainerName();
            const int dialogHeight = originalTrainerNameDropped ? 356 : 300;
            constexpr int dialogWidth = 580;
            int dialogX = 0, y = 0;
            int centerY = beginMoveConfirm(framebuffer, "要转换为第三世代格式吗？", Colors::Warning,
                                           dialogX, y, dialogWidth, dialogHeight);
            framebuffer.drawText(dialogX + 28, centerY, "第三世代没有性格字段，因此会重新生成 PID", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 28, "以保留原性格；转换结果可能会被判定为", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 56, "可能会被判定为非法，奖章与证章及", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 84, "携带道具也会被移除。此操作无法撤销。", Colors::Text);
            centerY += 124;
            if (originalTrainerNameDropped)
            {
                framebuffer.drawText(dialogX + 28, centerY, "第三世代无法写入这名训练家的名字，因此",
                                     Colors::Warning);
                framebuffer.drawText(dialogX + 28, centerY + 28, "初训家名字将留空。", Colors::Warning);
                centerY += 56;
            }
            framebuffer.drawText(dialogX + 28, centerY, "不建议这样操作，仍要继续吗？", Colors::TextDim,
                                 TextStyle::Caption);
            endMoveConfirm(screen, framebuffer, dialogX, y, dialogWidth, dialogHeight);
        }

        // Gen 1/2. Poke Transporter is the only way out of those games and it REBUILDS the Pokemon:
        // IVs are rerolled, a PID is invented, partial EXP is lost and the ability becomes the hidden
        // one. None of that can be undone, so it outranks the Let's Go notice when a move matches both;
        // like the other two it is shown only while the Move warning setting is on. It is also where
        // the user is told that the origin title is a guess,
        // because neither a Gen 1/2 record nor the save it came from holds a version byte.
        void drawVirtualConsoleTransferConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const bool originIsGuess = screen.pendingTransferOriginIsGuess();
            const int dialogHeight = originIsGuess ? 356 : 300;
            constexpr int dialogWidth = 600;
            int dialogX = 0, y = 0;
            int centerY = beginMoveConfirm(framebuffer, "宝可梦虚拟传送", Colors::Warning,
                                           dialogX, y, dialogWidth, dialogHeight);
            framebuffer.drawText(dialogX + 28, centerY, "离开第一／第二世代会重新生成个体值、创建", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 28, "PID、丢失部分经验和能力经验，",
                                 Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 56, "并赋予隐藏特性。此操作无法撤销。",
                                 Colors::Text);
            centerY += 96;
            if (originIsGuess)
            {
                // Naming the title we are about to stamp matters more than saying "unknown": the
                // user can see whether it matches the game the Pokemon actually came from, and
                // nothing else in PKSE will ever be able to tell them.
                const std::string originName =
                    Save::localizedGameVersionName(screen.pendingTransferOriginVersion());
                framebuffer.drawText(dialogX + 28, centerY, "没有任何信息记录它来自哪款游戏，因此",
                                     Colors::Warning);
                framebuffer.drawText(dialogX + 28, centerY + 28, "初训家游戏将设为" + originName + ".",
                                     Colors::Warning);
                centerY += 62;
            }
            framebuffer.drawText(dialogX + 28, centerY, "要继续传送吗？", Colors::TextDim,
                                 TextStyle::Caption);
            endMoveConfirm(screen, framebuffer, dialogX, y, dialogWidth, dialogHeight);
        }

        // Let's Go. Stat training is reset, which the player can earn back -- the least severe of the
        // three, so it is shown only when neither of the others applies.
        void drawLgpeTransferConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            constexpr int dialogWidth = 580, h = 300;
            int dialogX = 0, y = 0;
            int centerY =
                beginMoveConfirm(framebuffer, "Let's Go 传送", Colors::Primary, dialogX, y, dialogWidth, h);
            framebuffer.drawText(dialogX + 28, centerY, "传入或传出 Let's Go 会重置觉醒值／努力值", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 28, "为 0、移除携带道具，并删除它无法合法", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 56, "学会的招式。", Colors::Text);
            framebuffer.drawText(dialogX + 28, centerY + 96, "要继续传送吗？", Colors::TextDim,
                                 TextStyle::Caption);
            endMoveConfirm(screen, framebuffer, dialogX, y, dialogWidth, h);
        }

        void drawStorageExitConfirm(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            // The bank has unsaved changes; ask before leaving (HOME-style). Bank saving is separate
            // from the game (X) save, so this is the bank's own persistence decision.
            static const char *const items[] = {"保存并退出", "放弃更改", "取消"};
            drawPopupMenu(screen, framebuffer, "保存银行更改吗？", items, 3, screen.storageExitConfirmIndex);
        }
    }
}
