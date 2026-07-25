#include <algorithm>
#include <cmath>
#include <string>

#include "UI/Panels/BoxPokemonPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Pokemon/PokemonTypes.h"
#include "Names/ItemNames.h"
#include "Names/FormNames.h"   // getDisplayName -- variant prefix ("Alolan Raichu", "Combat Breed Tauros")

using namespace Trainer;

namespace UI {
namespace Panels {

    // Touch-button ids for the box header arrows (slot ids are 0..slotsPerBox-1).
    static constexpr int kPrevBoxId = 1000;
    static constexpr int kNextBoxId = 1001;
    static constexpr int kBoxNameId = 1002;   // tap the name pill to rename the box

    // Draw a type-icon sprite scaled to `h` px tall at (x, y); returns drawn width (0 if none).
    static int drawTypeIcon(PKSEFramebuffer& fb, Sprite* type, int x, int y, int h) {
        if (!type || !type->data) return 0;
        int w = (type->width * h) / type->height;
        fb.drawImageScaled(x, y, type->width, type->height, w, h, type->data, type->channels);
        return w;
    }

    // Pokemon HOME-style box: rounded card, indigo header with a centered name pill and ‹ ›
    // arrows, a grid of sprite-on-disc cells, a floating name-cursor on the selection, and a
    // slim selected-info strip at the bottom.
    void drawBoxPokemon(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height) {
        // Rounded card surface.
        fb.drawFilledRoundedRect(x, y, width, height, 16, Colors::Panel);

        // Box-slot / arrow touch targets are rebuilt every frame (hit-tested next frame).
        screen.touchButtons.clear();

        if (screen.selectedBoxIndex < 0 || screen.selectedBoxIndex >= static_cast<int>(screen.trainer.boxes.size())) {
            fb.drawRoundedRect(x, y, width, height, 16, Colors::Border, 1);
            fb.drawText(x + 20, y + 70, "没有可用的盒子数据", Colors::TextDim);
            return;
        }

        // ---- Header band (indigo, rounded top corners flush with the card) ----
        constexpr int headerH = 46;
        fb.drawFilledRoundedRect(x, y, width, headerH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + headerH - 16, width, 16, Colors::AccentDim);  // square the bottom edge

        // Centered box-name pill.
        std::string boxName = screen.selectedBoxIndex < static_cast<int>(screen.trainer.boxNames.size())
            ? screen.trainer.boxNames[screen.selectedBoxIndex]
            : ("盒子 " + std::to_string(screen.selectedBoxIndex + 1));
        int bnW, bnH; fb.measureText(boxName, bnW, bnH, TextStyle::Body);
        const int pillW = std::min(width - 160, bnW + 44);
        const int pillH = 30;
        const int pillX = x + (width - pillW) / 2;
        const int pillY = y + (headerH - pillH) / 2;
        // Amber pill when the header is focused (navigate up to it, or tap it, to rename). The name
        // is a touch target between the two arrow zones.
        const bool headerFocused = screen.detailViewActive && screen.selectedItemIndex == -1;
        fb.drawPill(pillX, pillY, pillW, pillH, headerFocused ? Colors::Primary : Colors::Panel);
        fb.drawText(x + (width - bnW) / 2, pillY + (pillH - bnH) / 2, boxName, headerFocused ? Colors::PrimaryText : Colors::Text);
        screen.touchButtons.push_back({ kBoxNameId, x + 64, y, width - 128, headerH });

        // ‹ › box arrows — geometric triangles via the symbol font (guaranteed glyphs, unlike
        // Nunito's ‹›); also touch targets -> L/R box change.
        const int arrowY = y + (headerH - bnH) / 2;
        fb.drawSymbol(x + 26, arrowY, "◀", Colors::Text);
        fb.drawSymbol(x + width - 40, arrowY, "▶", Colors::Text);
        screen.touchButtons.push_back({ kPrevBoxId, x, y, 64, headerH });
        screen.touchButtons.push_back({ kNextBoxId, x + width - 64, y, 64, headerH });

        // Box counter (small, right side of the band, left of the › arrow).
        std::string counter = std::to_string(screen.selectedBoxIndex + 1) + " / " +
                              std::to_string(screen.trainer.getBoxCount());
        int cW, cH; fb.measureText(counter, cW, cH, TextStyle::Caption);
        fb.drawText(x + width - 52 - cW, y + (headerH - cH) / 2, counter, Colors::Text, TextStyle::Caption);

        const auto& currentBox = screen.trainer.boxes[screen.selectedBoxIndex];

        // ---- Grid geometry (LGPE 5x5=25, others 6x5=30) ----
        const int slotsPerBox = static_cast<int>(screen.trainer.getSlotsPerBox());
        const int GRID_COLS = (slotsPerBox == 25) ? 5 : 6;
        const int GRID_ROWS = 5;

        const int gridTop = y + headerH + 8;
        const int gridBottom = y + height - 12;   // selection info now lives in the summary side-panel
        const int gridX = x + 18;
        const int gridW = width - 36;
        const int gridH = gridBottom - gridTop;
        const int colPitch = gridW / GRID_COLS;
        const int rowPitch = gridH / GRID_ROWS;
        int discR = std::min(colPitch, rowPitch) / 2 - 6;
        if (discR < 18) discR = 18;

        int selCx = 0, selTopY = 0;            // where to hang the name-cursor (drawn last, on top)
        std::string selLabel;
        bool haveSel = false;

        // Cursor scale-pop (Phase 4.6): restart the pop timer when the selection changes; the
        // selected disc briefly scales up (~16%) then settles over 0.14s.
        const double nowT = fb.getTimeSeconds();
        if (screen.detailViewActive && screen.selectedItemIndex != screen.animPrevBoxSlot) {
            screen.animBoxPopStart = nowT;
            screen.animPrevBoxSlot = screen.selectedItemIndex;
        }
        const double popRaw = 1.0 - (nowT - screen.animBoxPopStart) / 0.14;
        const double pop = popRaw > 0.0 ? popRaw : 0.0;

        for (int row = 0; row < GRID_ROWS; ++row) {
            for (int col = 0; col < GRID_COLS; ++col) {
                const int slotIndex = row * GRID_COLS + col;
                const int cellX = gridX + col * colPitch;
                const int cellY = gridTop + row * rowPitch;
                const int cx = cellX + colPitch / 2;
                const int cy = cellY + rowPitch / 2;

                // Whole-cell tap target (finger-friendly, larger than the disc).
                screen.touchButtons.push_back({ slotIndex, cellX, cellY, colPitch, rowPitch });

                const bool selected = screen.detailViewActive && slotIndex == screen.selectedItemIndex;
                const bool grabbed  = screen.swapActive &&
                                      screen.swapSourceBox == screen.selectedBoxIndex &&
                                      screen.swapSourceSlot == slotIndex;
                // Selected disc pops (scales up ~16%) briefly when the cursor lands on it.
                const int r = selected ? discR + static_cast<int>(std::lround(discR * 0.16 * pop)) : discR;

                const auto& pokemon = currentBox[slotIndex];
                std::string speciesName = pokemon ? std::string(pokemon->species()) : "";
                const bool empty = !pokemon || speciesName == "无" || speciesName == "空" ||
                                   pokemon->speciesID() == 0;

                // Selection halo (soft glow behind the disc).
                if (selected) fb.drawFilledCircle(cx, cy, r + 5, Color(Colors::Accent.r, Colors::Accent.g, Colors::Accent.b, 70));

                // Disc: occupied a touch lighter than empty for subtle depth.
                fb.drawFilledCircle(cx, cy, r, empty ? Colors::Panel : Colors::PanelAlt);

                if (empty) {
                    fb.drawCircle(cx, cy, r, Colors::Border, 1);
                } else {
                    const bool isShiny = pokemon->isShiny(pokemon->id32(), speciesName);
                    int sz = std::min(static_cast<int>(r * 1.8), colPitch - 6);
                    if (pokemon->isEgg()) {
                        // Eggs show as an egg in the grid; the summary panel still shows the species.
                        fb.drawEgg(cx, cy, sz);
                    } else {
                        Sprite* sprite = SpriteManager::getIconSprite(pokemon->speciesID(), pokemon->form(), isShiny);
                        if (sprite && sprite->data) {
                            fb.drawImageScaled(cx - sz / 2, cy - sz / 2, sprite->width, sprite->height,
                                               sz, sz, sprite->data, sprite->channels);
                        }
                    }
                    // Markers: partner heart (top-left), party number (bottom-left), shiny star (top-right).
                    if (screen.trainer.isStarterPokemon(screen.selectedBoxIndex, slotIndex))
                        fb.drawSymbol(cx - r, cy - r + 2, "♥", Colors::PartnerHeart);
                    int partyPos = screen.trainer.getPartyPosition(screen.selectedBoxIndex, slotIndex);
                    if (partyPos > 0) {
                        // Gold badge + dark digit (bottom-left), legible on any sprite in either theme.
                        const std::string n = std::to_string(partyPos);
                        const int bx = cx - r + 9, by = cy + r - 9;
                        fb.drawFilledCircle(bx, by, 9, Colors::PartyBadge);
                        int tw, th; fb.measureText(n, tw, th, TextStyle::Caption);
                        fb.drawText(bx - tw / 2, by - th / 2, n, Colors::PartyBadgeText, TextStyle::Caption);
                    }
                    if (isShiny)
                        fb.drawShinyMark(cx + r - 15, cy - r + 1, 15, Colors::ShinyStar);

                    if (selected) {
                        selCx = cx; selTopY = cy - r - 2; haveSel = true;
                        selLabel = Names::getDisplayName(pokemon->speciesID(), pokemon->form(), speciesName)
                                 + " (等级 " + std::to_string(pokemon->level()) + ")";
                    }
                }

                // Selection / grab ring on top of the disc.
                if (grabbed)       fb.drawCircle(cx, cy, r + 2, Colors::Primary, 3);
                else if (selected) fb.drawCircle(cx, cy, r + 2, Colors::Accent, 3);
            }
        }

        // Card border, then the floating HOME name-cursor over the selected disc (drawn last = on top).
        fb.drawRoundedRect(x, y, width, height, 16, Colors::Border, 1);
        if (haveSel) {
            // Theme-aware chip: indigo accent in light mode, near-black in dark mode; always light
            // text so it stays readable (a fixed dark chip + Colors::Text went invisible in light mode).
            const Color cursorFill = (g_themeMode == ThemeMode::Light) ? Colors::Accent : Color(24, 23, 32, 240);
            fb.drawNameCursorLabel(selCx, selTopY, selLabel, cursorFill, Colors::White);
        }
    }

    // Pokemon HOME-style summary side-panel (shown right of the box when entered): big idle HD
    // render, name/level/gender/shiny, type icons, the stat hexagon (actual stats), and a
    // Nature/Ability/Held Item card. Tapping it (id 2000) opens the full editor. Read-only display.
    void drawBoxSummaryPanel(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int width, int height) {
        fb.drawFilledRoundedRect(x, y, width, height, 16, Colors::Panel);

        // Header band.
        constexpr int headerH = 40;
        fb.drawFilledRoundedRect(x, y, width, headerH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + headerH - 16, width, 16, Colors::AccentDim);
        { int tw, th; fb.measureText("能力", tw, th, TextStyle::Body);
          fb.drawText(x + (width - tw) / 2, y + (headerH - th) / 2, "能力", Colors::Text); }

        // Resolve the selected Pokemon (must be entered + occupied).
        const Pokemon::Pokemon* p = nullptr;
        if (screen.detailViewActive &&
            screen.selectedBoxIndex >= 0 && screen.selectedBoxIndex < static_cast<int>(screen.trainer.boxes.size()) &&
            screen.selectedItemIndex >= 0 && screen.selectedItemIndex < static_cast<int>(screen.trainer.getSlotsPerBox())) {
            const auto& mon = screen.trainer.boxes[screen.selectedBoxIndex][screen.selectedItemIndex];
            if (mon && mon->speciesID() != 0) p = mon.get();
        }
        fb.drawRoundedRect(x, y, width, height, 16, Colors::Border, 1);
        if (!p) {
            int tw, th; fb.measureText("未选择宝可梦", tw, th, TextStyle::Caption);
            fb.drawText(x + (width - tw) / 2, y + height / 2, "未选择宝可梦", Colors::TextDim, TextStyle::Caption);
            return;
        }

        std::string species = std::string(p->species());
        const bool isShiny = p->isShiny(p->id32(), species);

        // HD render (idle), centered near the top.
        const int renderSz = 120;
        Sprite* sprite = SpriteManager::getSprite(p->speciesID(), p->form(), isShiny);
        if (sprite && sprite->data) {
            fb.drawSpriteIdle(x + (width - renderSz) / 2, y + headerH + 6, renderSz, renderSz,
                              sprite->width, sprite->height, sprite->data, sprite->channels, 0.0f);
        }

        int cy2 = y + headerH + 6 + renderSz + 4;
        // Name (Heading) + gender + shiny, centered. Variant forms read as "Alolan Raichu" etc.
        {
            std::string display = Names::getDisplayName(p->speciesID(), p->form(), species);
            int nW, nH; fb.measureText(display, nW, nH, TextStyle::Heading);
            fb.drawText(x + (width - nW) / 2, cy2, display, Colors::Text, TextStyle::Heading);
            int mx = x + (width + nW) / 2 + 6;
            const char* g = p->genderSymbol();
            if (g[0] != '\0') { fb.drawSymbol(mx, cy2 + 6, g, (std::string(g) == "♂") ? Colors::Blue : Colors::Magenta); mx += 20; }
            if (isShiny) fb.drawShinyMark(mx, cy2 + 6, 16, Colors::ShinyStar);
            cy2 += nH + 4;
        }
        // Lv + zero-padded dex no.
        {
            std::string dex = std::to_string(p->speciesID());
            while (dex.size() < 3) dex = "0" + dex;
            std::string sub = "等级 " + std::to_string(p->level()) + "    编号 " + dex;
            int sW, sH; fb.measureText(sub, sW, sH, TextStyle::Caption);
            fb.drawText(x + (width - sW) / 2, cy2, sub, Colors::TextDim, TextStyle::Caption);
            cy2 += 22;
        }
        // Type icons, centered.
        {
            Pokemon::TypePair types = Pokemon::getPokemonTypes(p->speciesID(), p->form());
            Sprite* t1 = SpriteManager::getTypeSprite(types.type1);
            Sprite* t2 = Pokemon::hasSecondType(types) ? SpriteManager::getTypeSprite(types.type2) : nullptr;
            const int th = 20;
            int w1 = (t1 && t1->data) ? (t1->width * th) / t1->height : 0;
            int w2 = (t2 && t2->data) ? (t2->width * th) / t2->height : 0;
            int gap = (w2 > 0) ? 8 : 0;
            int tx = x + (width - (w1 + gap + w2)) / 2;
            drawTypeIcon(fb, t1, tx, cy2, th); tx += w1 + gap;
            drawTypeIcon(fb, t2, tx, cy2, th);
            cy2 += th + 10;
        }

        // Stat hexagon (actual stats, HOME vertex order [HP, Atk, Def, Spe, SpD, SpA]).
        float vals[6] = {
            static_cast<float>(p->statHPMax()), static_cast<float>(p->statATK()), static_cast<float>(p->statDEF()),
            static_cast<float>(p->statSPE()),   static_cast<float>(p->statSPD()), static_cast<float>(p->statSPA())
        };
        const int hexCx = x + width / 2;
        const int hexR = 70;
        // Center the hexagon well below the type badges: its top vertex + HP label extend
        // ~(hexR + 14 + lineHeight) above center, so a small offset would collide with the types.
        const int hexCy = cy2 + hexR + 32;
        fb.drawStatHexagon(hexCx, hexCy, hexR, vals, 6, 255.0f,
                           Color(Colors::Accent.r, Colors::Accent.g, Colors::Accent.b, 110),
                           Colors::Border, Colors::Accent);
        // Vertex labels: "ABBR value", anchored by side.
        static const char* abbr[6] = { "HP", "攻击", "防御", "速度", "特防", "特攻" };
        static const double ang[6]  = { 90, 30, -30, 270, 210, 150 };
        const double PI = 3.14159265358979323846;
        for (int i = 0; i < 6; ++i) {
            double a = ang[i] * PI / 180.0;
            int vx = hexCx + static_cast<int>(std::lround(std::cos(a) * (hexR + 14)));
            int vy = hexCy - static_cast<int>(std::lround(std::sin(a) * (hexR + 14)));
            std::string lbl = std::string(abbr[i]) + " " + std::to_string(static_cast<int>(vals[i]));
            int lw, lh; fb.measureText(lbl, lw, lh, TextStyle::Caption);
            double c = std::cos(a);
            int lx = (c > 0.3) ? vx + 4 : (c < -0.3) ? vx - 4 - lw : vx - lw / 2;
            int ly = (i == 0) ? vy - lh : (i == 3) ? vy : vy - lh / 2;
            fb.drawText(lx, ly, lbl, Colors::Text, TextStyle::Caption);
        }

        // Quick info: Nature / Ability / Held Item (label left, value right).
        // Start below the bottom (Speed) hexagon label so they don't collide.
        int iy = hexCy + hexR + 34;
        auto infoRow = [&](const char* label, const std::string& value) {
            fb.drawText(x + 18, iy, label, Colors::TextDim, TextStyle::Caption);
            int vw, vh; fb.measureText(value, vw, vh, TextStyle::Caption);
            fb.drawText(x + width - 18 - vw, iy, value, Colors::Text, TextStyle::Caption);
            iy += 20;
        };
        infoRow("性格",    getNatureName(p->nature()));
        infoRow("特性",   getAbilityName(p->ability()));
        infoRow("携带道具", p->heldItem()
            ? std::string(p->getGameGroup() == Enums::GameVersion::FRLG
                ? Names::getItemNameG3(p->heldItem()) : getItemName(p->heldItem()))
            : std::string("无"));

        // Edit hint + whole-panel tap target (opens the editor for the selected slot).
        const char* hint = "A／点击：编辑";
        int hw, hh; fb.measureText(hint, hw, hh, TextStyle::Caption);
        fb.drawText(x + (width - hw) / 2, y + height - 24, hint, Colors::Accent, TextStyle::Caption);
        screen.touchButtons.push_back({ 2000, x, y + headerH, width, height - headerH });
    }
}
}
