#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "UI/Panels/HomeMenuPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"

using namespace Trainer;

namespace UI {
namespace Panels {

    // Count live (non-empty) Pokemon in a boxes container.
    template <typename Boxes>
    static int countMons(const Boxes& boxes) {
        int n = 0;
        for (const auto& box : boxes)
            for (const auto& mon : box)
                if (mon && mon->speciesID() != 0) ++n;
        return n;
    }

    // Draw a compact, non-interactive preview of the current box (disc + sprite per slot).
    static void drawBoxPreview(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int w, int h) {
        constexpr int headerH = 40;
        fb.drawFilledRoundedRect(x, y, w, headerH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + headerH - 16, w, 16, Colors::AccentDim);

        const int box = screen.selectedBoxIndex;
        const bool inRange = box >= 0 && box < static_cast<int>(screen.trainer.boxes.size());
        std::string boxName = (inRange && box < static_cast<int>(screen.trainer.boxNames.size()))
            ? screen.trainer.boxNames[box]
            : ("盒子 " + std::to_string(box + 1));
        int bnW, bnH; fb.measureText(boxName, bnW, bnH, TextStyle::Body);
        fb.drawText(x + (w - bnW) / 2, y + (headerH - bnH) / 2, boxName, Colors::Text);
        if (!inRange) return;

        const int slotsPerBox = static_cast<int>(screen.trainer.getSlotsPerBox());
        const int cols = (slotsPerBox == 25) ? 5 : 6;
        const int rows = 5;
        const int gx = x + 16, gy = y + headerH + 10;
        const int gw = w - 32, gh = h - headerH - 20;
        const int colPitch = gw / cols, rowPitch = gh / rows;
        int discR = std::min(colPitch, rowPitch) / 2 - 5;
        if (discR < 14) discR = 14;

        const auto& curBox = screen.trainer.boxes[box];
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const int idx = r * cols + c;
                const int cx = gx + c * colPitch + colPitch / 2;
                const int cy = gy + r * rowPitch + rowPitch / 2;
                const auto& mon = curBox[idx];
                const bool empty = !mon || mon->speciesID() == 0;
                fb.drawFilledCircle(cx, cy, discR, empty ? Colors::Panel : Colors::PanelAlt);
                if (empty) { fb.drawCircle(cx, cy, discR, Colors::Border, 1); continue; }
                int sz = std::min(static_cast<int>(discR * 1.8), colPitch - 4);
                if (mon->isEgg()) {
                    fb.drawEgg(cx, cy, sz);  // eggs show as an egg in the box preview too
                } else {
                    bool shiny = mon->isShiny(mon->id32(), mon->species());
                    Sprite* sp = SpriteManager::getIconSprite(mon->speciesID(), mon->form(), shiny);
                    if (sp && sp->data) {
                        fb.drawImageScaled(cx - sz / 2, cy - sz / 2, sp->width, sp->height, sz, sz, sp->data, sp->channels);
                    }
                }
            }
        }
    }

    // Simple vector icons for the menu's circular buttons (no icon assets). `col` = glyph color,
    // `bg` = the button fill (used to punch holes). kind: 0 Items (bag), 1 Trainer (person), 2 Settings (gear).
    static void drawMenuIcon(PKSEFramebuffer& fb, int cx, int cy, int r, int kind, Color col, Color bg) {
        if (kind == 0) {            // Items: a satchel/bag
            const int bw = 2 * r, bh = 2 * r - 4;
            const int bx = cx - bw / 2, by = cy - bh / 2 + 4;
            fb.drawFilledRoundedRect(bx, by, bw, bh, 8, col);                    // body
            fb.drawFilledRoundedRect(cx - r + 4, by - 8, 2 * r - 8, 14, 6, col); // flap
            fb.drawFilledRoundedRect(cx - 3, cy - 2, 6, bh / 2, 3, bg);          // clasp (punch)
        } else if (kind == 1) {     // Trainer: a person bust
            fb.drawFilledCircle(cx, cy - r / 2, r / 2 - 1, col);                 // head
            const int sw = 2 * r - 6, sh = r + 2;
            fb.drawFilledRoundedRect(cx - sw / 2, cy + 2, sw, sh, sw / 2, col);  // shoulders
        } else {                    // Settings: a gear
            const double PI = 3.14159265358979323846;
            for (int i = 0; i < 8; ++i) {
                double a = i * PI / 4.0;
                int tx = cx + static_cast<int>(std::lround(std::cos(a) * (r - 2)));
                int ty = cy + static_cast<int>(std::lround(std::sin(a) * (r - 2)));
                fb.drawFilledCircle(tx, ty, 4, col);                            // teeth
            }
            fb.drawFilledCircle(cx, cy, r - 6, col);                            // body
            fb.drawFilledCircle(cx, cy, (r - 6) / 2, bg);                       // hole
        }
    }

    void drawHomeMenu(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        screen.touchButtons.clear();

        // ---------- Left: live box-preview card ----------
        const int pX = 32, pY = 96, pW = 560, pH = 540;
        fb.drawSoftShadow(pX, pY, pW, pH, 16);
        fb.drawFilledRoundedRect(pX, pY, pW, pH, 16, Colors::Panel);
        drawBoxPreview(screen, fb, pX, pY, pW, pH);
        fb.drawRoundedRect(pX, pY, pW, pH, 16, Colors::Border, 1);

        // ---------- Right: destination pills ----------
        const int rX = 648, rW = 600;
        const int stored = countMons(screen.trainer.boxes);
        int party = 0;
        for (const auto& m : screen.trainer.party) if (m && m->speciesID() != 0) ++party;
        int bankN = 0;
        if (screen.bank) bankN = countMons(screen.bank->boxes);

        struct Pill { const char* label; std::string sub; int idx; };
        Pill pills[3] = {
            { "宝可梦", std::to_string(stored) + " 个已存储", 0 },
            { "同行宝可梦",   std::to_string(party)  + " / 6",    1 },
            { "存储", std::to_string(bankN)  + " 在银行中", 2 },
        };
        const int pillH = 76, pillGap = 18;
        int py = 108;
        for (const auto& p : pills) {
            const bool focused = (screen.homeMenuIndex == p.idx);
            fb.drawSoftShadow(rX, py, rW, pillH, pillH / 2);
            fb.drawPill(rX, py, rW, pillH, focused ? Colors::Primary : Colors::PanelAlt);
            const Color txt = focused ? Colors::PrimaryText : Colors::Text;
            if (focused) fb.drawSymbol(rX - 30, py + pillH / 2 - 12, "\xE2\x96\xB6", Colors::Primary);  // ▶ pointer

            int lW, lH; fb.measureText(p.label, lW, lH, TextStyle::Heading);
            fb.drawText(rX + 36, py + (pillH - lH) / 2, p.label, txt, TextStyle::Heading);

            // Count sub-pill on the right.
            int sW, sH; fb.measureText(p.sub, sW, sH, TextStyle::Caption);
            const int subW = sW + 28, subH = 30;
            const int subX = rX + rW - subW - 20, subY = py + (pillH - subH) / 2;
            fb.drawPill(subX, subY, subW, subH, focused ? Colors::PrimaryText : Colors::Panel);
            fb.drawText(subX + 14, subY + (subH - sH) / 2, p.sub, focused ? Colors::Primary : Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.push_back({ 100 + p.idx, rX, py, rW, pillH });
            py += pillH + pillGap;
        }

        // ---------- Right: circular icon row (Items / Trainer / Settings) ----------
        struct Icon { const char* label; const char* glyph; int idx; };
        Icon icons[3] = { { "道具", "I", 3 }, { "训练家", "T", 4 }, { "设置", "S", 5 } };
        const int iconR = 42;
        const int slot = rW / 3;
        const int iconTop = py + 22;
        for (int j = 0; j < 3; ++j) {
            const auto& ic = icons[j];
            const int cx = rX + slot * j + slot / 2;
            const int cy = iconTop + iconR;
            const bool focused = (screen.homeMenuIndex == ic.idx);
            if (focused) fb.drawFilledCircle(cx, cy, iconR + 5, Color(Colors::Primary.r, Colors::Primary.g, Colors::Primary.b, 70));
            fb.drawFilledCircle(cx, cy, iconR, focused ? Colors::Primary : Colors::PanelAlt);
            fb.drawCircle(cx, cy, iconR, focused ? Colors::Primary : Colors::Border, 2);

            drawMenuIcon(fb, cx, cy, iconR - 12, j, focused ? Colors::PrimaryText : Colors::Text,
                         focused ? Colors::Primary : Colors::PanelAlt);

            int lW, lH; fb.measureText(ic.label, lW, lH, TextStyle::Caption);
            fb.drawText(cx - lW / 2, cy + iconR + 8, ic.label, focused ? Colors::Primary : Colors::TextDim, TextStyle::Caption);

            screen.touchButtons.push_back({ 100 + ic.idx, cx - iconR, cy - iconR, 2 * iconR, 2 * iconR + 24 });
        }
    }
}
}
