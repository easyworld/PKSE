#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

#include "UI/Panels/StoragePanel.h"
#include "UI/Panels/CarriedSprite.h" // drawLiftedMon -- shared with the Boxes view
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "UI/ScreenChrome.h" // drawGlyphButton -- on-button controller glyphs
#include "UI/Dialogs/DialogFrame.h"
#include "Names/FormNames.h" // getDisplayName -- variant prefix ("Alolan Raichu", etc.)
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"

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
            void drawSlotDisc(TrainerViewScreen &screen, PKSEFramebuffer &fb,
                              const Pokemon::Pokemon *pk, bool savePane, int boxIndex, int slotIndex,
                              int cx, int cy, int discR)
            {
                if (!pk || pk->speciesID() == 0)
                    return;

                const std::string speciesName(pk->species());
                const bool isShiny = pk->isShiny(pk->id32(), speciesName);

                const int sz = static_cast<int>(discR * 1.75);
                if (pk->isEgg())
                {
                    fb.drawEgg(cx, cy, sz); // eggs show as an egg in the grid
                }
                else
                {
                    Sprite *sprite = SpriteManager::getIconSprite(pk->speciesID(), pk->form(), isShiny);
                    if (sprite && sprite->data)
                        fb.drawImageScaled(cx - sz / 2, cy - sz / 2, sprite->width, sprite->height, sz, sz, sprite->data, sprite->channels);
                }

                if (isShiny)
                    fb.drawShinyMark(cx + discR - 15, cy - discR + 1, 15, Colors::ShinyStar); // top-right

                // Party-membership badge (save pane only): partner heart (top-left) or party number (bottom-left).
                if (savePane)
                {
                    if (screen.trainer.isStarterPokemon(boxIndex, slotIndex))
                    {
                        fb.drawSymbol(cx - discR, cy - discR + 2, "\xE2\x99\xA5", Colors::PartnerHeart);
                    }
                    else
                    {
                        int partyPos = screen.trainer.getPartyPosition(boxIndex, slotIndex);
                        if (partyPos > 0)
                        {
                            // Gold badge + dark digit at the disc's bottom-left, legible on any sprite/theme.
                            const std::string n = std::to_string(partyPos);
                            const int bx = cx - discR + 9, by = cy + discR - 9;
                            fb.drawFilledCircle(bx, by, 9, Colors::PartyBadge);
                            int tw, th;
                            fb.measureText(n, tw, th, TextStyle::Caption);
                            fb.drawText(bx - tw / 2, by - th / 2, n, Colors::PartyBadgeText, TextStyle::Caption);
                        }
                    }
                }
            }

            // Draws one storage pane in the HOME box style (rounded card + indigo header band + disc
            // grid), and reports its cell geometry via outGeom.
            void drawPane(TrainerViewScreen &screen, PKSEFramebuffer &fb,
                          int px, int py, int pw, int ph, bool savePane,
                          int boxIndex, int cursorSlot, bool focused, bool entered,
                          int cols, int rows, int slotsPerBox,
                          const std::string &label, int boxCount, PaneGeom &outGeom)
            {
                fb.drawFilledRoundedRect(px, py, pw, ph, 16, Colors::Panel);

                // Header band (rounded top). Focused pane = accent indigo; unfocused = dim.
                constexpr int headerH = 44;
                const Color band = focused ? Colors::AccentDim : Colors::Border;
                fb.drawFilledRoundedRect(px, py, pw, headerH, 16, band);
                fb.drawFilledRect(px, py + headerH - 16, pw, 16, band);

                int lw, lh;
                fb.measureText(label, lw, lh, TextStyle::Body);
                const int pillW = std::min(pw - 120, lw + 40), pillH = 28;
                const int pillX = px + (pw - pillW) / 2, pillY = py + (headerH - pillH) / 2;
                // Amber pill when the header itself is focused (navigate up to it, or tap it, to rename
                // this box). cursorSlot == -1 is the "header focused" sentinel.
                const bool headerFocused = focused && entered && cursorSlot == -1;
                fb.drawPill(pillX, pillY, pillW, pillH, headerFocused ? Colors::Primary : Colors::Panel);
                fb.drawText(px + (pw - lw) / 2, pillY + (pillH - lh) / 2, label, headerFocused ? Colors::PrimaryText : Colors::Text);

                const int arrowY = py + (headerH - lh) / 2;
                fb.drawSymbol(px + 18, arrowY, "\xE2\x97\x80", focused ? Colors::Text : Colors::TextDim);      // left
                fb.drawSymbol(px + pw - 32, arrowY, "\xE2\x96\xB6", focused ? Colors::Text : Colors::TextDim); // right
                // Tappable box arrows (special slot ids -2 = prev box, -3 = next box) and the name pill
                // (-4 = rename this box). The name target sits between the two arrow zones.
                screen.storageTouchTargets.push_back({savePane ? 0 : 1, boxIndex, -2, px, py, 64, headerH});
                screen.storageTouchTargets.push_back({savePane ? 0 : 1, boxIndex, -3, px + pw - 64, py, 64, headerH});
                screen.storageTouchTargets.push_back({savePane ? 0 : 1, boxIndex, -4, px + 64, py, pw - 128, headerH});
                std::string counter = std::to_string(boxIndex + 1) + " / " + std::to_string(boxCount);
                int cw, ch;
                fb.measureText(counter, cw, ch, TextStyle::Caption);
                fb.drawText(px + pw - 44 - cw, py + (headerH - ch) / 2, counter, focused ? Colors::Text : Colors::TextDim, TextStyle::Caption);

                const int gridTop = py + headerH + 8;
                const int gridX = px + 14, gridW = pw - 28, gridH = ph - headerH - 18;
                const int colPitch = gridW / cols, rowPitch = gridH / rows;
                int discR = std::min(colPitch, rowPitch) / 2 - 4;
                if (discR < 12)
                    discR = 12;
                outGeom = PaneGeom{gridX, gridTop, colPitch, rowPitch, discR, cols, rows,
                                   px + pw / 2, pillY};

                const int thisPane = savePane ? 0 : 1;

                // The rubber-band rectangle being swept out in Multi mode: a green wash UNDER the
                // Pokemon (so they stay readable) with a matching border on top. It spans from the
                // anchor cell to the cell under the cursor, the way HOME's multi-select highlight does.
                int selC0 = -1, selR0 = -1, selC1 = -1, selR1 = -1;
                if (screen.currentlySelecting && entered && screen.selectPane == thisPane &&
                    screen.selectBox == boxIndex && cursorSlot >= 0)
                {
                    const int ax = screen.selectDimensions.first, ay = screen.selectDimensions.second;
                    const int cxs = cursorSlot % cols, cys = cursorSlot / cols;
                    selC0 = std::min(ax, cxs);
                    selC1 = std::max(ax, cxs);
                    selR0 = std::min(ay, cys);
                    selR1 = std::max(ay, cys);
                    const Color wash(Colors::CursorMulti.r, Colors::CursorMulti.g, Colors::CursorMulti.b, 64);
                    fb.drawFilledRoundedRect(gridX + selC0 * colPitch + 2, gridTop + selR0 * rowPitch + 2,
                                             (selC1 - selC0 + 1) * colPitch - 4, (selR1 - selR0 + 1) * rowPitch - 4,
                                             14, wash);
                }

                for (int i = 0; i < slotsPerBox; ++i)
                {
                    const int row = i / cols, col = i % cols;
                    const int cellX = gridX + col * colPitch, cellY = gridTop + row * rowPitch;
                    const int cx = cellX + colPitch / 2, cy = cellY + rowPitch / 2;

                    // Whole-cell touch target (consumed next frame in update()).
                    screen.storageTouchTargets.push_back({thisPane, boxIndex, i, cellX, cellY, colPitch, rowPitch});

                    const Pokemon::Pokemon *pk = savePane ? screen.trainer.boxes[boxIndex][i].get()
                                                          : (screen.bank ? screen.bank->boxes[boxIndex][i].get() : nullptr);
                    const bool empty = !pk || pk->speciesID() == 0;

                    fb.drawFilledCircle(cx, cy, discR, empty ? Colors::Panel : Colors::PanelAlt);
                    if (empty)
                        fb.drawCircle(cx, cy, discR, Colors::Border, 1);

                    drawSlotDisc(screen, fb, pk, savePane, boxIndex, i, cx, cy, discR);
                }

                if (selC0 >= 0)
                {
                    fb.drawRoundedRect(gridX + selC0 * colPitch + 2, gridTop + selR0 * rowPitch + 2,
                                       (selC1 - selC0 + 1) * colPitch - 4, (selR1 - selR0 + 1) * rowPitch - 4,
                                       14, Colors::CursorMulti, 3);
                }
            }

            // The carried block plus the pointer cursor, drawn on top of BOTH panes so a group stays
            // fully visible while it travels across the screen and over the pane boundary.
            void drawCarryAndCursor(TrainerViewScreen &screen, PKSEFramebuffer &fb, const PaneGeom &g, int cursorSlot)
            {
                const Color cur = cursorColorFor(screen.cursorMode);
                // Gentle vertical bob, a HOME-style pointer wobble done as a sine so it is framerate-independent.
                const int bob = static_cast<int>(std::sin(fb.getTimeSeconds() * 3.4) * 3.0);
                // The head is symmetric about its point, so the point goes straight on the slot's
                // centre line -- no horizontal nudge needed.

                // The box-name pill is a cursor position of its own (slot -1): navigate up off the top
                // row onto it and A renames the box. Point at it with the same arrow so the cursor is
                // never invisible, rather than relying on the pill's amber highlight alone.
                if (cursorSlot < 0)
                {
                    fb.drawPointerCursor(g.pillCx, g.pillTop - 2 + bob, kGridCursorH, cur);
                    return; // the header is unreachable while carrying
                }

                if (screen.carrying())
                {
                    const int w = std::max(1, screen.selectDimensions.first);
                    const int h = std::max(1, screen.selectDimensions.second);
                    const bool fits = screen.checkPutDownBounds();
                    // A group that would hang off the grid is tinted with the warning colour, so the
                    // "must land in the exact slots" rule is visible before you press A rather than after.
                    const Color tint = fits ? cur : Colors::Warning;
                    const int baseX = g.cellX(cursorSlot), baseY = g.cellY(cursorSlot);
                    for (int y = 0; y < h; ++y)
                    {
                        for (int x = 0; x < w; ++x)
                        {
                            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                            if (idx >= screen.moveMon.size())
                                continue;
                            const int cx = baseX + x * g.colPitch + g.colPitch / 2;
                            const int cy = baseY + y * g.rowPitch + g.rowPitch / 2 + bob;
                            // Backing tile marks the footprint the block will occupy -- including its
                            // holes, which stay empty when it lands.
                            if (w > 1 || h > 1)
                                fb.drawFilledRoundedRect(cx - g.colPitch / 2 + 3, cy - g.rowPitch / 2 + 3,
                                                         g.colPitch - 6, g.rowPitch - 6, 14,
                                                         Color(tint.r, tint.g, tint.b, 70));
                            drawLiftedMon(fb, screen.moveMon[idx].get(), cx, cy, g.discR);
                        }
                    }
                    if (w > 1 || h > 1)
                        fb.drawRoundedRect(baseX + 3, baseY + 3 + bob, w * g.colPitch - 6, h * g.rowPitch - 6,
                                           14, tint, 3);
                }

                // The cursor itself, in the active mode's colour: its point rests on the top of the
                // slot's disc rather than in the middle of it, so the Pokemon, its shiny mark and its
                // party badge all stay visible. On the top row the body runs up over the box-name pill
                // -- deliberately, the way HOME's cursor does; it is drawn last, so it floats over the
                // header instead of being clipped by it.
                const int cx = g.centerX(cursorSlot), cy = g.centerY(cursorSlot);
                fb.drawPointerCursor(cx, cy - g.discR - 3 + bob, kGridCursorH, cur);
            }
        }

        void drawStorageView(TrainerViewScreen &screen, PKSEFramebuffer &fb, int x, int y, int width, int height)
        {
            if (!screen.bank)
            {
                fb.drawCard(x, y, width, height);
                fb.drawText(x + 16, y + 70, "银行不可用", Colors::TextDim);
                return;
            }

            const bool entered = screen.detailViewActive; // cursor only shows once the view is entered
            screen.storageTouchTargets.clear();           // rebuilt each frame by drawPane below

            constexpr int gap = 16;
            constexpr int infoH = 60;
            const int paneW = (width - gap) / 2;
            const int paneH = height - infoH - 8;

            const int saveSlots = static_cast<int>(screen.trainer.getSlotsPerBox());
            const int saveCols = (saveSlots == 25) ? 5 : 6;
            const int bankSlots = static_cast<int>(Bank::BANK_SLOTS_PER_BOX);
            const int bankCols = 6;
            const int rows = 5;

            const bool saveFocused = screen.storageFocusPane == 0;
            const bool bankFocused = screen.storageFocusPane == 1;

            std::string saveLabel = screen.stSaveBox < static_cast<int>(screen.trainer.boxNames.size())
                                        ? screen.trainer.boxNames[screen.stSaveBox]
                                        : ("盒子 " + std::to_string(screen.stSaveBox + 1));
            std::string bankLabel = screen.bank->boxDisplayName(screen.stBankBox);

            PaneGeom saveGeom, bankGeom;

            drawPane(screen, fb, x, y, paneW, paneH, /*savePane*/ true,
                     screen.stSaveBox, screen.stSaveSlot, saveFocused, entered,
                     saveCols, rows, saveSlots, saveLabel, static_cast<int>(screen.trainer.getBoxCount()), saveGeom);

            drawPane(screen, fb, x + paneW + gap, y, paneW, paneH, /*savePane*/ false,
                     screen.stBankBox, screen.stBankSlot, bankFocused, entered,
                     bankCols, rows, bankSlots, bankLabel, static_cast<int>(Bank::BANK_BOX_COUNT), bankGeom);

            // Cursor + carried block last, over BOTH panes: a group being carried from one side to the
            // other must not be painted over by the destination pane's card.
            if (entered)
            {
                drawCarryAndCursor(screen, fb, saveFocused ? saveGeom : bankGeom,
                                   saveFocused ? screen.stSaveSlot : screen.stBankSlot);
            }

            // --- Info strip ---
            const int iy = y + paneH + 8;
            fb.drawCard(x, iy, width, infoH);

            if (!entered)
            {
                fb.drawText(x + 16, iy + infoH / 2 - 11, "按 A 打开存储界面", Colors::TextDim, TextStyle::Body);
                return;
            }

            // Mode swatch (dot) + name.
            const Color modeCol = cursorColorFor(screen.cursorMode);
            fb.drawFilledCircle(x + 22, iy + infoH / 2, 8, modeCol);
            fb.drawText(x + 38, iy + infoH / 2 - 11, modeName(screen.cursorMode), Colors::Text, TextStyle::Body);

            // Right side: how big the group in hand (or being swept out) is, in the mode colour.
            std::string sel;
            if (screen.carrying() && screen.carriedCount() > 1)
            {
                sel = std::to_string(screen.carriedCount()) + " 只拿起中（" + std::to_string(screen.selectDimensions.first) + "×" + std::to_string(screen.selectDimensions.second) + "）";
            }
            else if (screen.currentlySelecting)
            {
                sel = "选择中 - 按 A 拿起整组";
            }
            if (!sel.empty())
            {
                int sw, sh;
                fb.measureText(sel, sw, sh);
                fb.drawText(x + width - 16 - sw, iy + infoH / 2 - 11, sel, Colors::CursorMulti, TextStyle::Body);
            }

            // Middle: the held Pokemon, else the one under the focused cursor. When the box-name header
            // is focused (slot == -1) there is no cursor mon, so prompt the rename instead of indexing.
            const int focusSlot = saveFocused ? screen.stSaveSlot : screen.stBankSlot;
            const bool holding = screen.carrying();
            const Pokemon::Pokemon *focus = screen.firstCarried();
            if (!focus && focusSlot >= 0)
            {
                focus = saveFocused ? screen.trainer.boxes[screen.stSaveBox][focusSlot].get()
                                    : screen.bank->boxes[screen.stBankBox][focusSlot].get();
            }

            const int tx = x + 120;
            if (focusSlot == -1 && !holding)
            {
                fb.drawText(tx, iy + infoH / 2 - 11, "已选择盒子名称 - 按 A 重命名", Colors::TextDim, TextStyle::Body);
            }
            else if (holding && screen.carriedCount() > 1)
            {
                // A whole group in hand: name the leader and say how many ride with it, rather than
                // pretending one Pokemon is the whole payload.
                std::string display = Names::getDisplayName(focus->speciesID(), focus->form(), std::string(focus->species()));
                std::string line = "拿起中：" + display + " +" + std::to_string(screen.carriedCount() - 1) + (screen.checkPutDownBounds() ? "   按 A 放在此处" : "   此处空间不足");
                fb.drawText(tx, iy + infoH / 2 - 11, line,
                            screen.checkPutDownBounds() ? Colors::Text : Colors::Warning, TextStyle::Body);
            }
            else if (focus && focus->speciesID() != 0)
            {
                std::string name(focus->species());
                const bool shiny = focus->isShiny(focus->id32(), name);
                std::string display = Names::getDisplayName(focus->speciesID(), focus->form(), name);
                std::string line = (holding ? "拿起中：" : "") + display + "   等级 " + std::to_string(focus->level());
                fb.drawText(tx, iy + infoH / 2 - 11, line, Colors::Text, TextStyle::Body);
                int lw, lh;
                fb.measureText(line, lw, lh, TextStyle::Body);
                int mx = tx + lw + 10;
                const char *g = focus->genderSymbol();
                if (g[0] != '\0')
                {
                    fb.drawSymbol(mx, iy + infoH / 2 - 11, g, (std::string(g) == "\xE2\x99\x82") ? Colors::Blue : Colors::Magenta);
                    mx += 20;
                }
                if (shiny)
                    fb.drawShinyMark(mx, iy + infoH / 2 - 11, 16, Colors::ShinyStar);
            }
            else
            {
                fb.drawText(tx, iy + infoH / 2 - 11, "空槽位", Colors::TextDim, TextStyle::Body);
            }
        }

        namespace
        {
            // A small centered popup menu (scrim + card + title + item list). Each row is registered as
            // a touch button (id = item index) so a tap selects + confirms it.
            void drawPopupMenu(TrainerViewScreen &screen, PKSEFramebuffer &fb, const std::string &title,
                               const char *const *items, int count, int sel, uint32_t disabledMask = 0)
            {
                // Touch-friendly sizing: 56px rows (TouchTargetMin) and a wide card so rows are easy to tap.
                constexpr int w = 440, rowH = TouchTargetMin;
                constexpr int headerH = 60;
                const int h = headerH + count * rowH + 14;
                const int x = (fb.getWidth() - w) / 2;
                const int y = (fb.getHeight() - h) / 2;
                constexpr int r = Dialogs::kDialogRadius; // same card shape as every other modal
                fb.drawFilledRect(0, 0, fb.getWidth(), fb.getHeight(), Color(0, 0, 0, 130));
                fb.drawSoftShadow(x, y, w, h, r);
                fb.drawFilledRoundedRect(x, y, w, h, r, Colors::Panel);
                fb.drawRoundedRect(x, y, w, h, r, Colors::Border, 1);
                fb.drawText(x + 22, y + 16, title, Colors::Text, TextStyle::Heading);
                fb.drawFilledRect(x + r, y + headerH - 4, w - r * 2, 2, Colors::Accent);
                screen.touchButtons.clear();
                int ry = y + headerH;
                for (int i = 0; i < count; ++i)
                {
                    const bool disabled = (disabledMask >> i) & 1u;
                    const bool selRow = (i == sel) && !disabled;
                    if (selRow)
                        fb.drawSelectionHighlight(x + 10, ry + 3, w - 20, rowH - 6);
                    int tw, th;
                    fb.measureText(items[i], tw, th, TextStyle::Body);
                    const Color rowColor = disabled ? Colors::Border : (selRow ? Colors::Text : Colors::TextDim);
                    fb.drawText(x + 28, ry + (rowH - th) / 2, items[i], rowColor, TextStyle::Body);
                    if (!disabled)
                        screen.touchButtons.push_back({i, x + 10, ry, w - 20, rowH}); // greyed rows aren't tappable
                    ry += rowH;
                }
            }
        }

        void drawStorageActionMenu(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            const Pokemon::Pokemon *pk = screen.storageSlot(screen.menuPane, screen.menuBox, screen.menuSlot).get();
            const std::string title = (pk && pk->speciesID() != 0)
                                          ? Names::getDisplayName(pk->speciesID(), pk->form(), pk->species())
                                          : "宝可梦";
            static const char *const items[] = {"招式", "编辑", "复制", "放生", "取消"};
            // A party-linked (LGPE) slot can be edited or cloned, but not moved or released -- grey those
            // two out. (Same lock rule as storageSlotLocked: save pane + a party member points here.)
            const bool locked = screen.menuPane == 0 &&
                                screen.trainer.getPartyPosition(screen.menuBox, screen.menuSlot) > 0;
            const uint32_t disabled = locked ? ((1u << 0) | (1u << 3)) : 0u; // Move (0), Release (3)
            drawPopupMenu(screen, fb, title, items, 5, screen.storageMenuIndex, disabled);
        }

        // Options for the block in hand. There is deliberately no "move" entry: a carried group is moved
        // by carrying it to the destination and pressing A, which is what makes placement positional.
        void drawStorageGroupMenu(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            const int n = screen.carriedCount();
            const std::string title = std::to_string(n) + (n == 1 ? " 只宝可梦拿起中" : " 只宝可梦拿起中");
            static const char *const items[] = {"全部放生", "放回原来的位置", "取消"};
            drawPopupMenu(screen, fb, title, items, 3, screen.groupMenuIndex);
        }

        void drawStorageReleaseConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            std::string msg;
            if (screen.releaseGroup)
            {
                msg = "放生" + std::to_string(screen.carriedCount()) + "只宝可梦？";
            }
            else
            {
                const Pokemon::Pokemon *pk = screen.storageSlot(screen.releasePane, screen.releaseBox, screen.releaseSlot).get();
                const std::string who = (pk && pk->speciesID() != 0)
                                            ? Names::getDisplayName(pk->speciesID(), pk->form(), pk->species())
                                            : "这只宝可梦";
                msg = "放生" + who + "?";
            }
            constexpr int w = 540, h = 226;
            const int x = (fb.getWidth() - w) / 2;
            const int y = (fb.getHeight() - h) / 2;
            int cy = Dialogs::drawDialogFrame(fb, x, y, w, h, "放生", Colors::Red);
            fb.drawText(x + 28, cy, msg, Colors::Text);
            fb.drawText(x + 28, cy + 34, "此操作无法撤销。", Colors::TextDim, TextStyle::Caption);

            // Buttons carry their glyph (B: Cancel, A: Release); Release stays red.
            screen.touchButtons.clear();
            const int bw = 190, bh = TouchTargetMin, by = y + h - bh - 18;
            const int relX = x + w - bw - 20;   // right = Release (id 1 -> A)
            const int cancelX = relX - bw - 16; // left  = Cancel  (id 0 -> B)
            drawGlyphButton(fb, cancelX, by, bw, bh, "B", "取消", Colors::PanelAlt);
            screen.touchButtons.push_back({0, cancelX, by, bw, bh});
            drawGlyphButton(fb, relX, by, bw, bh, "A", "放生", Colors::Red, Colors::White);
            screen.touchButtons.push_back({1, relX, by, bw, bh});
        }

        // Creator "保留这只新宝可梦吗？" confirm. Three glyph buttons (like the Edit Item dialog),
        // so each is one controller button rather than a Left/Right selector: B = Back (return to
        // editing), Y = Discard (remove it), A = Keep. ids 0 / 2 / 1 respectively.
        void drawCreatorKeepConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            constexpr int w = 540, h = 226;
            const int x = (fb.getWidth() - w) / 2;
            const int y = (fb.getHeight() - h) / 2;
            int cy = Dialogs::drawDialogFrame(fb, x, y, w, h, "新建宝可梦", Colors::Primary);
            fb.drawText(x + 28, cy, "保留这只新宝可梦吗？", Colors::Text);
            fb.drawText(x + 28, cy + 34, "放弃将移除它；返回可继续编辑。", Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.clear();
            const int cbw = 160, cbh = TouchTargetMin, cby = y + h - cbh - 18;
            drawGlyphButton(fb, x + 24, cby, cbw, cbh, "B", "返回", Colors::PanelAlt);
            screen.touchButtons.push_back({0, x + 24, cby, cbw, cbh});
            drawGlyphButton(fb, x + (w - cbw) / 2, cby, cbw, cbh, "Y", "放弃", Colors::PanelAlt);
            screen.touchButtons.push_back({2, x + (w - cbw) / 2, cby, cbw, cbh});
            drawGlyphButton(fb, x + w - 24 - cbw, cby, cbw, cbh, "A", "保留", Colors::PanelAlt);
            screen.touchButtons.push_back({1, x + w - 24 - cbw, cby, cbw, cbh});
        }

        void drawDetailsDiscardConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            // Deliberately the same shape as the creator's Keep/Discard above: same three glyph
            // buttons in the same places, so leaving an edit page always looks and answers alike.
            // What differs is the stakes -- here the mon already exists and only the EDITS are at
            // risk, so the safe action (Save) sits on A where Keep sits for a new mon.
            constexpr int w = 560, h = 226;
            const int x = (fb.getWidth() - w) / 2;
            const int y = (fb.getHeight() - h) / 2;
            int cy = Dialogs::drawDialogFrame(fb, x, y, w, h, "未保存的更改", Colors::Orange);
            fb.drawText(x + 28, cy, "这只宝可梦有尚未保存的更改。", Colors::Text);
            fb.drawText(x + 28, cy + 34, "放弃将丢失这些更改；返回可继续编辑。",
                        Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.clear();
            const int cbw = 160, cbh = TouchTargetMin, cby = y + h - cbh - 18;
            drawGlyphButton(fb, x + 24, cby, cbw, cbh, "B", "返回", Colors::PanelAlt);
            screen.touchButtons.push_back({0, x + 24, cby, cbw, cbh});
            drawGlyphButton(fb, x + (w - cbw) / 2, cby, cbw, cbh, "Y", "放弃", Colors::PanelAlt);
            screen.touchButtons.push_back({2, x + (w - cbw) / 2, cby, cbw, cbh});
            drawGlyphButton(fb, x + w - 24 - cbw, cby, cbw, cbh, "A", "保存", Colors::PanelAlt);
            screen.touchButtons.push_back({1, x + w - 24 - cbw, cby, cbw, cbh});
        }

        namespace
        {
            // Frame + Cancel/Continue buttons shared by the two lossy-move notices. Only the chrome is
            // shared; each dialog writes its own copy, because they warn about different losses.
            // Returns the content Y to start drawing body text at.
            int beginMoveConfirm(PKSEFramebuffer &fb,
                                 const char *title, Color accent, int &x, int &y, int w, int h)
            {
                x = (fb.getWidth() - w) / 2;
                y = (fb.getHeight() - h) / 2;
                return Dialogs::drawDialogFrame(fb, x, y, w, h, title, accent);
            }

            void endMoveConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb, int x, int y, int w, int h)
            {
                // On-button glyphs (B: Cancel, A: Continue) -- a single button each, no Left/Right selector.
                screen.touchButtons.clear();
                const int bw = 190, bh = TouchTargetMin, by = y + h - bh - 18;
                const int contX = x + w - bw - 20;   // right = Continue (id 1 -> A)
                const int cancelX = contX - bw - 16; // left  = Cancel   (id 0 -> B)
                drawGlyphButton(fb, cancelX, by, bw, bh, "B", "取消", Colors::PanelAlt);
                screen.touchButtons.push_back({0, cancelX, by, bw, bh});
                drawGlyphButton(fb, contX, by, bw, bh, "A", "继续", Colors::PanelAlt);
                screen.touchButtons.push_back({1, contX, by, bw, bh});
            }
        }

        // FireRed/LeafGreen. Down-converting rebuilds the PID to keep the nature (Gen 3 has no nature
        // field), which is destructive and cannot be undone -- so this is never gated by the Move
        // warning setting, and it supersedes the Let's Go notice when a move is both.
        void drawGen3ConvertConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            constexpr int w = 580, h = 300;
            int x = 0, y = 0;
            int cy = beginMoveConfirm(fb, "要转换为第三世代格式吗？", Colors::Warning, x, y, w, h);
            fb.drawText(x + 28, cy, "第三世代没有性格字段，因此会重新生成 PID", Colors::Text);
            fb.drawText(x + 28, cy + 28, "以保留原性格；转换结果可能会被判定为", Colors::Text);
            fb.drawText(x + 28, cy + 56, "非法，昵称、奖章与证章及携带道具也会", Colors::Text);
            fb.drawText(x + 28, cy + 84, "被移除。此操作无法撤销。", Colors::Text);
            fb.drawText(x + 28, cy + 124, "不建议这样操作，仍要继续吗？", Colors::TextDim, TextStyle::Caption);
            endMoveConfirm(screen, fb, x, y, w, h);
        }

        // Let's Go. Stat training is reset, which the player can earn back -- so this one obeys the
        // Move warning setting.
        void drawLgpeTransferConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            constexpr int w = 580, h = 300;
            int x = 0, y = 0;
            int cy = beginMoveConfirm(fb, "Let's Go 传送", Colors::Primary, x, y, w, h);
            fb.drawText(x + 28, cy, "传入或传出 Let's Go 会重置觉醒值／努力值", Colors::Text);
            fb.drawText(x + 28, cy + 28, "为 0、移除携带道具，并删除它无法合法", Colors::Text);
            fb.drawText(x + 28, cy + 56, "学会的招式。", Colors::Text);
            fb.drawText(x + 28, cy + 96, "要继续传送吗？", Colors::TextDim, TextStyle::Caption);
            endMoveConfirm(screen, fb, x, y, w, h);
        }

        void drawStorageExitConfirm(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            // The bank has unsaved changes; ask before leaving (HOME-style). Bank saving is separate
            // from the game (X) save, so this is the bank's own persistence decision.
            static const char *const items[] = {"保存并退出", "放弃更改", "取消"};
            drawPopupMenu(screen, fb, "保存银行更改吗？", items, 3, screen.storageExitConfirmIndex);
        }
    }
}
