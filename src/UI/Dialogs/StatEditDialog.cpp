#include "UI/Dialogs/StatEditDialog.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"

using namespace Trainer;

namespace UI {
namespace Dialogs {
void drawStatEditDialog(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        // Draw dialog
        int dialogWidth = 600;
        int dialogHeight = 300;
        int dialogX = (fb.getWidth() - dialogWidth) / 2;
        int dialogY = (fb.getHeight() - dialogHeight) / 2;

        // Draw dialog background
        fb.drawFilledRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Panel);
        fb.drawRect(dialogX, dialogY, dialogWidth, dialogHeight, Colors::Border);

        // Get the Pokemon being edited based on whether it's party or box Pokemon
        const Pokemon::Pokemon* pokemon = nullptr;
        if (screen.pokemonDetailsIsParty) {
            // Get from party
            if (screen.pokemonDetailsPartyIndex >= 0 && screen.pokemonDetailsPartyIndex < static_cast<int>(screen.trainer.party.size())) {
                pokemon = screen.trainer.party[screen.pokemonDetailsPartyIndex].get();
            }
        } else {
            // Get from boxes
            if (screen.selectedBoxIndex < 0 || screen.selectedBoxIndex >= static_cast<int>(screen.trainer.boxes.size())) return;
            if (screen.selectedItemIndex < 0 || screen.selectedItemIndex >= static_cast<int>(BOX_SLOTS)) return;

            const auto& boxPokemon = screen.trainer.boxes[screen.selectedBoxIndex][screen.selectedItemIndex];
            if (boxPokemon) {
                pokemon = boxPokemon.get();
            }
        }

        if (!pokemon) return;

        // Stat names
        const char* statNames[] = {"HP", "攻击", "防御", "特攻", "特防", "速度"};
        const char* currentStatName = statNames[screen.statEditSelectedStat];

        // Get current EV value
        uint8_t currentEV = 0;

        switch (screen.statEditSelectedStat) {
            case 0: currentEV = pokemon->evHP(); break;
            case 1: currentEV = pokemon->evATK(); break;
            case 2: currentEV = pokemon->evDEF(); break;
            case 3: currentEV = pokemon->evSPA(); break;
            case 4: currentEV = pokemon->evSPD(); break;
            case 5: currentEV = pokemon->evSPE(); break;
        }

        // Calculate total EVs
        int totalEVs = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
            pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD();

        // Draw title
        std::string titleText = std::string("编辑能力值 - ") + currentStatName;
        fb.drawText(dialogX + 20, dialogY + 20, titleText, Colors::Text);
        fb.drawFilledRect(dialogX + 20, dialogY + 45, dialogWidth - 40, 2, Colors::Border);

        int lineY = dialogY + 70;
        int lineHeight = 30;

        // Show current stat name
        fb.drawText(dialogX + 20, lineY, std::string("能力: ") + currentStatName, Colors::Text);
        lineY += lineHeight;

        // Show IV value (always show current edited value)
        char ivText[64];
        snprintf(ivText, sizeof(ivText), "个体值:  %2d  (最小: 0, 最大: 31)", screen.statEditCurrentIV);
        Color ivColor = screen.statEditMode == StatEditMode::IV ? Colors::Yellow : Colors::Text;
        if (screen.statEditMode == StatEditMode::IV) {
            fb.drawText(dialogX + 15, lineY, "", Colors::Yellow);
        }
        fb.drawText(dialogX + 40, lineY, ivText, ivColor);
        lineY += lineHeight;

        // Show EV value (always show current edited value)
        char evText[64];
        snprintf(evText, sizeof(evText), "努力值:  %3d (最小: 0, 最大: 252)", screen.statEditCurrentEV);
        Color evColor = screen.statEditMode == StatEditMode::EV ? Colors::Yellow : Colors::Text;
        if (screen.statEditMode == StatEditMode::EV) {
            fb.drawText(dialogX + 15, lineY, "", Colors::Yellow);
        }
        fb.drawText(dialogX + 40, lineY, evText, evColor);
        lineY += lineHeight;

        // Show total EVs
        lineY += 10;
        char totalText[64];

        // Calculate what the new total would be using current edited EV
        int projectedTotal = totalEVs - currentEV + screen.statEditCurrentEV;

        snprintf(totalText, sizeof(totalText), "努力值总和: %d/510", projectedTotal);
        Color totalColor = projectedTotal > 510 ? Colors::Red : Colors::TextDim;
        fb.drawText(dialogX + 20, lineY, totalText, totalColor);

        if (projectedTotal > 510) {
            fb.drawText(dialogX + 220, lineY, "(超出上限！)", Colors::Red);
        }
        lineY += lineHeight + 15;
    }
}
}
