#include <string>

#include "UI/Dialogs/StatEditDialog.h"
#include "UI/Dialogs/DialogFrame.h"
#include "UI/Dialogs/EditControls.h"   // shared step buttons + A/B choice buttons
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"
#include "Globals.h"

using namespace Trainer;

namespace UI {
namespace Dialogs {
    void drawStatEditDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        // Resolve the Pokemon being edited (party / box / bank).
        const Pokemon::Pokemon* pokemon = screen.detailsTargetPokemon();
        if (!pokemon) return;

        // Let's Go uses Awakening Values (AVs) instead of EVs (EVs are inert in that game),
        // so the second editable stat is AV for LGPE Pokemon and EV for everything else.
        const bool usesAV = pokemon->hasAwakeningValues();

        const char* statNames[] = {"HP", "攻击", "防御", "特攻", "特防", "速度"};
        const char* statName = statNames[screen.statEdit.selectedStat];

        constexpr int w = 600, h = 384;
        const int x = (fb.getWidth() - w) / 2;
        const int y = (fb.getHeight() - h) / 2;
        int cy = drawDialogFrame(fb, x, y, w, h, std::string("编辑") + statName, Colors::Text);

        screen.touchButtons.clear();

        const bool ivMode = (screen.statEdit.mode == StatEditMode::IV);
        const int labelX = x + 28, valX = x + 150, rangeX = x + w - 150;

        // IV row (tap -> select IV, id 10)
        if (ivMode) fb.drawSelectionHighlight(x + 16, cy - 4, w - 32, 34);
        fb.drawText(labelX, cy + 2, "IV", ivMode ? Colors::Text : Colors::TextDim);
        fb.drawText(valX,   cy + 2, std::to_string(screen.statEdit.currentIV), ivMode ? Colors::Accent : Colors::Text);
        fb.drawText(rangeX, cy + 4, "(0 - 31)", Colors::TextDim, TextStyle::Caption);
        screen.touchButtons.push_back({10, x + 16, cy - 4, w - 32, 34});
        cy += 42;

        // Second row: AV (Let's Go) or EV (tap -> select it, id 11)
        if (!ivMode) fb.drawSelectionHighlight(x + 16, cy - 4, w - 32, 34);
        fb.drawText(labelX, cy + 2, usesAV ? "AV" : "EV", !ivMode ? Colors::Text : Colors::TextDim);
        fb.drawText(valX,   cy + 2, std::to_string(usesAV ? screen.statEdit.currentAV : screen.statEdit.currentEV),
                    !ivMode ? Colors::Accent : Colors::Text);
        fb.drawText(rangeX, cy + 4, usesAV ? (g_allowIllegalEdits ? "(0 - 255)" : "(0 - 200)")
                                          : (g_allowIllegalEdits ? "(0 - 255)" : "(0 - 252)"),
                    Colors::TextDim, TextStyle::Caption);
        screen.touchButtons.push_back({11, x + 16, cy - 4, w - 32, 34});
        cy += 44;

        if (usesAV) {
            fb.drawText(labelX, cy, "觉醒值（每项 0-200，无总和上限）", Colors::TextDim, TextStyle::Caption);
        } else {
            uint8_t currentEV = 0;
            switch (screen.statEdit.selectedStat) {
                case 0: currentEV = pokemon->evHP();  break;
                case 1: currentEV = pokemon->evATK(); break;
                case 2: currentEV = pokemon->evDEF(); break;
                case 3: currentEV = pokemon->evSPA(); break;
                case 4: currentEV = pokemon->evSPD(); break;
                case 5: currentEV = pokemon->evSPE(); break;
            }
            int totalEVs = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
                           pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD();
            int projectedTotal = totalEVs - currentEV + screen.statEdit.currentEV;
            bool over = projectedTotal > 510;
            fb.drawText(labelX, cy, "努力值总和：" + std::to_string(projectedTotal) + " / 510",
                        over ? Colors::Red : Colors::TextDim, TextStyle::Caption);
        }

        // Step buttons (ZL -100 | L -10 | < -1 | > +1 | R +10 | ZR +100), then Cancel / Save labelled
        // with B / A. Up/Down still switch the IV <-> EV/AV row (handled in the input path).
        const int cbh = TouchTargetMin, cby = y + h - cbh - 14;
        drawEditStepRow(screen, fb, x, w, cby - 58 - 14, 58);

        const int cbw = 190;
        drawEditChoiceButton(screen, fb, x + 24, cby, cbw, cbh, "B", "取消", 0);
        drawEditChoiceButton(screen, fb, x + w - 24 - cbw, cby, cbw, cbh, "A", "保存", 1);
    }
}
}
