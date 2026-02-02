#include <cstdio>
#include <string>

#include "UI/Panels/PartyPokemonPanel.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/PokemonTypes.h"

using namespace Trainer;

namespace UI {
namespace Panels {
    void drawPartyPokemon(PKSEFramebuffer& fb, const std::vector<std::unique_ptr<Pokemon::Pokemon>>& party, uint32_t trainerID32, int x, int y, int width, int height, int selectedIndex) {
        fb.drawFilledRect(x, y, width, height, Colors::Panel);
        fb.drawRect(x, y, width, height, Colors::Border);

        fb.drawText(x + 20, y + 20, "Party Pokemon", Colors::Text);
        fb.drawFilledRect(x + 20, y + 45, width - 40, 2, Colors::Border);

        // Draw in two columns: Slot 1-3 on left, Slot 4-6 on right
        int lineHeight = 20;
        int columnWidth = (width - 60) / 2;  // Dynamic column width based on panel width

        for (size_t i = 0; i < party.size() && i < 6; i++) {
            const Pokemon::Pokemon* pokemon = party[i].get();
            if (!pokemon) continue;

            // Determine column (0 = left, 1 = right)
            int column = (i >= 3) ? 1 : 0;
            int slotInColumn = (i >= 3) ? (i - 3) : i;

            // Calculate position
            int colX = x + 20 + (column * columnWidth);
            int colY = y + 60 + (slotInColumn * 170);  // Each slot takes ~170px vertically

            // Highlight selected Pokemon
            if (selectedIndex >= 0 && static_cast<int>(i) == selectedIndex) {
                fb.drawFilledRect(colX - 5, colY - 5, columnWidth - 10, 165, Colors::Selected);
            }

            if (pokemon->speciesID() == 0) {
                std::string slotText = "栏位 " + std::to_string(i + 1) + ": 空";
                fb.drawText(colX, colY, slotText, Colors::TextDim);
                continue;
            }

            // Load Pokemon sprite (form-aware) - will draw later at bottom of cell
            bool isShiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
            Sprite* sprite = SpriteManager::getSprite(pokemon->speciesID(), pokemon->form(), isShiny);

            // Draw header: "Slot X: Species"
            std::string headerText = "栏位 " + std::to_string(i + 1) + ": " + std::string(pokemon->species());
            int textX = colX;
            fb.drawText(colX, colY, headerText, Colors::Text);
            textX += headerText.length() * 8;  // Approximate character width

            // Draw gender symbol next to species name
            std::string genderSymbol = pokemon->genderSymbol();
            if (genderSymbol != "?" && genderSymbol != "Genderless" && std::string(genderSymbol) != "") {
                Color genderColor = (genderSymbol == "♂") ? Colors::Blue : Colors::Magenta;
                fb.drawText(textX, colY, std::string(" ") + genderSymbol, genderColor);
                textX += 16;  // Space + symbol width
            }

            // Draw shiny star in red after gender
            // if (pokemon->isShiny(trainerID32, pokemon->species())) {
            if (pokemon->isShiny(pokemon->id32(), pokemon->species())) {
                fb.drawText(textX, colY, " ★", Colors::Red);
                textX += 16;
            }

            // Draw level
            std::string levelText = " - Lv." + std::to_string(pokemon->level());
            fb.drawText(textX, colY, levelText, Colors::Text);

            int TYPE_SPRITE_HEIGHT = 14;
            Pokemon::TypePair types = Pokemon::getPokemonTypes(pokemon->speciesID(), pokemon->form());
            Sprite* type1Sprite = SpriteManager::getTypeSprite(types.type1);
            Sprite* type2Sprite = Pokemon::hasSecondType(types) ? SpriteManager::getTypeSprite(types.type2) : nullptr;

            // Calculate total width needed for types
            int type1Width = 0, type2Width = 0;
            if (type1Sprite && type1Sprite->data) {
                type1Width = (type1Sprite->width * TYPE_SPRITE_HEIGHT) / type1Sprite->height;
            }
            if (type2Sprite && type2Sprite->data) {
                type2Width = (type2Sprite->width * TYPE_SPRITE_HEIGHT) / type2Sprite->height;
            }
            // int totalTypeWidth = type1Width + (type2Width > 0 ? 5 + type2Width : 0);

            // Position types at fixed position
            int typeX = colX + 280;
            if (type1Sprite && type1Sprite->data) {
                fb.drawImageScaled(typeX, colY, type1Sprite->width, type1Sprite->height,
                    type1Width, TYPE_SPRITE_HEIGHT,
                    type1Sprite->data, type1Sprite->channels);
                typeX += type1Width + 5;
            }
            if (type2Sprite && type2Sprite->data) {
                fb.drawImageScaled(typeX, colY, type2Sprite->width, type2Sprite->height,
                    type2Width, TYPE_SPRITE_HEIGHT,
                    type2Sprite->data, type2Sprite->channels);
            }

            colY += lineHeight;

            // Draw stats header
            fb.drawText(colX + 20, colY, "    Base | IV | EV  | Stat", Colors::TextDim);
            colY += lineHeight;

            // Draw each stat
            char statLine[100];
            snprintf(statLine, sizeof(statLine), "HP : %03d | %02d | %03d | %03d",
                pokemon->baseHP(), pokemon->ivHP(), pokemon->evHP(), pokemon->statHPMax());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);
            colY += lineHeight;

            snprintf(statLine, sizeof(statLine), "ATK: %03d | %02d | %03d | %03d",
                pokemon->baseATK(), pokemon->ivATK(), pokemon->evATK(), pokemon->statATK());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);
            colY += lineHeight;

            snprintf(statLine, sizeof(statLine), "DEF: %03d | %02d | %03d | %03d",
                pokemon->baseDEF(), pokemon->ivDEF(), pokemon->evDEF(), pokemon->statDEF());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);
            colY += lineHeight;

            snprintf(statLine, sizeof(statLine), "SPA: %03d | %02d | %03d | %03d",
                pokemon->baseSPA(), pokemon->ivSPA(), pokemon->evSPA(), pokemon->statSPA());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);
            colY += lineHeight;

            snprintf(statLine, sizeof(statLine), "SPD: %03d | %02d | %03d | %03d",
                pokemon->baseSPD(), pokemon->ivSPD(), pokemon->evSPD(), pokemon->statSPD());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);
            colY += lineHeight;

            snprintf(statLine, sizeof(statLine), "SPE: %03d | %02d | %03d | %03d",
                pokemon->baseSPE(), pokemon->ivSPE(), pokemon->evSPE(), pokemon->statSPE());
            fb.drawText(colX + 20, colY, statLine, Colors::Text);

            // Draw Pokemon sprite at bottom-right of cell
            if (sprite && sprite->data) {
                int cellStartY = y + 60 + (((i >= 3) ? (i - 3) : i) * 170);
                int spriteX = colX + 280;
                int spriteY = cellStartY + 60;
                fb.drawImage(spriteX, spriteY, sprite->width, sprite->height,
                    sprite->data, sprite->channels);
            }
        }
    }
}
}
