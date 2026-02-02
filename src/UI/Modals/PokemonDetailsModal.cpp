#include <cstring>

#include "UI/Modals/PokemonDetailsModal.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Utils/HelperUtilities.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/PokemonTypes.h"
#include "Names/FormNames.h"
#include "Names/TypeNames.h"

using namespace Trainer;
using namespace Utils;

namespace UI {
namespace Modals {
    void drawPokemonDetailsModal(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        // Get the selected pokemon based on whether it's party or box Pokemon
        const Pokemon::Pokemon* pokemon = nullptr;
        if (screen.pokemonDetailsIsParty) {
            // Get from party
            if (screen.pokemonDetailsPartyIndex >= 0 && screen.pokemonDetailsPartyIndex < static_cast<int>(screen.trainer.party.size())) {
                pokemon = screen.trainer.party[screen.pokemonDetailsPartyIndex].get();
                if (pokemon->speciesID() == 0) return; // Empty slot
            } else {
                return;
            }
        } else {
            // Get from boxes
            if (screen.selectedBoxIndex < 0 || screen.selectedBoxIndex >= static_cast<int>(screen.trainer.boxes.size())) return;
            if (screen.selectedItemIndex < 0 || screen.selectedItemIndex >= static_cast<int>(BOX_SLOTS)) return;

            const auto& boxPokemon = screen.trainer.boxes[screen.selectedBoxIndex][screen.selectedItemIndex];
            if (!boxPokemon) return; // Empty slot
            pokemon = boxPokemon.get();
        }

        if (!pokemon) return;

        // Modal dimensions - take up most of the screen
        constexpr int MODAL_WIDTH = 1100;
        constexpr int MODAL_HEIGHT = 600;
        const int modalX = (fb.getWidth() - MODAL_WIDTH) / 2;
        const int modalY = (fb.getHeight() - MODAL_HEIGHT) / 2;

        // Draw modal background
        fb.drawFilledRect(modalX, modalY, MODAL_WIDTH, MODAL_HEIGHT, Colors::Panel);
        fb.drawRect(modalX, modalY, MODAL_WIDTH, MODAL_HEIGHT, Colors::Border);

        // Left panel - Categories
        constexpr int CATEGORY_PANEL_WIDTH = 200;
        fb.drawRect(modalX + 10, modalY + 10, CATEGORY_PANEL_WIDTH, MODAL_HEIGHT - 20, Colors::Border);

        const char* categories[] = {"主要", "遇见", "能力", "招式", "外观", "OT/其他"};
        int catY = modalY + 30;
        for (int i = 0; i < 6; i++) {
            if (i == screen.pokemonDetailsCategory) {
                fb.drawFilledRect(modalX + 15, catY - 2, CATEGORY_PANEL_WIDTH - 10, 22, Colors::Selected);
            }
            std::string catText = std::string("> ") + categories[i];
            // TODO: We need to dim categories that cannot be edited at the moment
            // Main and Stats are the only categories, currently, that are editable
            UI::Color color = (i == 0 || i == 2 ? Colors::Text : Colors::TextDim);
            fb.drawText(modalX + 25, catY, catText, color);
            catY += 30;
        }

        // Right panel - Content area
        const int contentX = modalX + CATEGORY_PANEL_WIDTH + 30;
        const int contentY = modalY + 20;
        const int contentWidth = MODAL_WIDTH - CATEGORY_PANEL_WIDTH - 50;

        // Load and draw Pokemon sprite on the right side
        bool isShiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
        Sprite* pokemonSprite = SpriteManager::getSprite(pokemon->speciesID(), pokemon->form(), isShiny);

        constexpr int SPRITE_DISPLAY_SIZE = 120;  // Size to display the sprite
        int spriteX = modalX + MODAL_WIDTH - SPRITE_DISPLAY_SIZE - 30;
        int spriteY = modalY + 30;

        if (pokemonSprite && pokemonSprite->data) {
            // Calculate scaled dimensions to fit within SPRITE_DISPLAY_SIZE while maintaining aspect ratio
            int scaledWidth = SPRITE_DISPLAY_SIZE;
            int scaledHeight = (pokemonSprite->height * SPRITE_DISPLAY_SIZE) / pokemonSprite->width;

            if (scaledHeight > SPRITE_DISPLAY_SIZE) {
                scaledHeight = SPRITE_DISPLAY_SIZE;
                scaledWidth = (pokemonSprite->width * SPRITE_DISPLAY_SIZE) / pokemonSprite->height;
            }

            fb.drawImageScaled(spriteX, spriteY, pokemonSprite->width, pokemonSprite->height,
                scaledWidth, scaledHeight, pokemonSprite->data, pokemonSprite->channels);
        }

        // TODO: We need to calculate the amount of fields and then pass it to the TrainerViewScreen for iteration when active
        // Draw category content
        if (screen.pokemonDetailsCategory == 0) { // Main
            int lineY = contentY;
            int lineHeight = 25;

            fb.drawText(contentX, lineY, "=== 主要信息 ===", Colors::Text);
            lineY += lineHeight + 5;

            char buffer[128];
            // TODO: We set the text color to DIM if the field cannot be edited (future updates will allow editing)
            // Field 0: PID
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 0) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "PID: %08X", pokemon->pid());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 1: Species
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 1) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            // Display form name
            const char* formName = Names::getFormName(pokemon->speciesID(), pokemon->form());
            if (formName && strlen(formName) > 0) {
                snprintf(buffer, sizeof(buffer), "种类: %s (%s) (#%d)",
                    pokemon->species(), formName, pokemon->speciesID());
            } else {
                snprintf(buffer, sizeof(buffer), "种类: %s (#%d)",
                    pokemon->species(), pokemon->speciesID());
            }
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 2: Type (non-editable)
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 2) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            {
                // Get Pokemon types for this species and form
                Pokemon::TypePair types = Pokemon::getPokemonTypes(pokemon->speciesID(), pokemon->form());
                std::string typeText = "属性: ";
                fb.drawText(contentX, lineY, typeText, Colors::TextDim);

                int typeTextWidth = typeText.length() * 8;
                Sprite* type1Sprite = SpriteManager::getTypeSprite(types.type1);

                int spriteX = contentX + typeTextWidth + 10;
                constexpr int TYPE_SPRITE_HEIGHT = 14;

                if (type1Sprite && type1Sprite->data) {
                    int scaledWidth = (type1Sprite->width * TYPE_SPRITE_HEIGHT) / type1Sprite->height;
                    fb.drawImageScaled(spriteX, lineY - 2, type1Sprite->width, type1Sprite->height,
                        scaledWidth, TYPE_SPRITE_HEIGHT, type1Sprite->data, type1Sprite->channels);
                    spriteX += scaledWidth + 5;
                }

                if (Pokemon::hasSecondType(types)) {
                    Sprite* type2Sprite = SpriteManager::getTypeSprite(types.type2);
                    if (type2Sprite && type2Sprite->data) {
                        int scaledWidth = (type2Sprite->width * TYPE_SPRITE_HEIGHT) / type2Sprite->height;
                        fb.drawImageScaled(spriteX, lineY - 2, type2Sprite->width, type2Sprite->height,
                            scaledWidth, TYPE_SPRITE_HEIGHT, type2Sprite->data, type2Sprite->channels);
                    }
                }
            }
            lineY += lineHeight;

            // Field 3: Gender
            std::string genderSymbol = pokemon->genderSymbol();
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 3) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            if (genderSymbol == "") {
                std::string genderText = "性别: 无性别";
                fb.drawText(contentX, lineY, genderText, Colors::TextDim);
            }
            else {
                std::string genderText = "性别: ";
                Color genderColor = (genderSymbol == "♂") ? Colors::Blue : Colors::Magenta;
                fb.drawText(contentX, lineY, genderText, Colors::TextDim);
                fb.drawText(contentX + 55, lineY, genderSymbol, genderColor);
            }
            lineY += lineHeight;

            // Field 4: Shiny
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 4) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "异色: %s", pokemon->isShiny(pokemon->id32(), pokemon->species()) ? "是" : "否");
            Color shinyColor = screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 4 ? Colors::Yellow : Colors::Text;
            fb.drawText(contentX, lineY, buffer, shinyColor);
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 4) {
                fb.drawText(contentX + 150, lineY, "(按A切换)", Colors::TextDim);
            }
            lineY += lineHeight;

            // Field 5: Nickname
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 5) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            std::string nickname = utf16ToUtf8(pokemon->nickname());
            snprintf(buffer, sizeof(buffer), "昵称: %s", nickname.c_str());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 6: EXP
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 6) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "经验值: %u", pokemon->exp());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 7: Level
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 7) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "等级: %d", pokemon->level());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 8: Nature
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 8) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "性格: %s (%d)", getNatureName(pokemon->nature()), pokemon->nature());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 9: Stat Nature
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 9) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "能力性格: %s (%d)", getNatureName(pokemon->statNature()), pokemon->statNature());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 10: Held Item
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 10) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "携带道具: %s (%d)", getItemName(pokemon->heldItem()), pokemon->heldItem());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 11: Ability
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 11) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "特性: %s (%d)", getAbilityName(pokemon->ability()), pokemon->ability());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 12: Friendship Value (0-255)
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 12) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "亲密度: %d", pokemon->friendship());
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 13: Whether this Pokemon is an egg
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 13) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            snprintf(buffer, sizeof(buffer), "是否为蛋: %s", pokemon->isEgg() ? "是" : "否");
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);
            lineY += lineHeight;

            // Field 14: Whether this Pokemon is infected, cured or has not been/is not infected with Pokerus
            if (screen.pokemonDetailsEditing && screen.pokemonDetailsSelectedField == 14) {
                fb.drawText(contentX - 15, lineY, ">", Colors::Yellow);
            }
            const char* pkrsStatus = pokemon->isPokerusInfected() ? "已感染" :
                pokemon->isPokerusCured() ? "已治愈" : "无";
            snprintf(buffer, sizeof(buffer), "宝可病毒: %s", pkrsStatus);
            fb.drawText(contentX, lineY, buffer, Colors::TextDim);

        } else if (screen.pokemonDetailsCategory == 2) { // Stats
            int lineY = contentY;
            int lineHeight = 25;

            fb.drawText(contentX, lineY, "=== 能力值 ===", Colors::Text);
            lineY += lineHeight + 5;

            // Header
            fb.drawText(contentX + 31, lineY, "种族 | 个体 | 努力 | 能力", Colors::TextDim);
            lineY += lineHeight;

            // Calculate totals
            int ivTotal = pokemon->ivHP() + pokemon->ivATK() + pokemon->ivDEF() +
                pokemon->ivSPE() + pokemon->ivSPA() + pokemon->ivSPD();
            int evTotal = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
                pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD();
            int baseTotal = pokemon->baseHP() + pokemon->baseATK() + pokemon->baseDEF() +
                pokemon->baseSPE() + pokemon->baseSPA() + pokemon->baseSPD();
            int statTotal = pokemon->statHPMax() + pokemon->statATK() + pokemon->statDEF() +
                pokemon->statSPE() + pokemon->statSPA() + pokemon->statSPD();

            // Draw each stat
            const char* statNames[] = {"HP ", "攻击", "防御", "特攻", "特防", "速度"};
            uint8_t baseStats[] = {pokemon->baseHP(), pokemon->baseATK(), pokemon->baseDEF(),
                pokemon->baseSPA(), pokemon->baseSPD(), pokemon->baseSPE()};
            uint8_t ivs[] = {pokemon->ivHP(), pokemon->ivATK(), pokemon->ivDEF(),
                pokemon->ivSPA(), pokemon->ivSPD(), pokemon->ivSPE()};
            uint8_t evs[] = {pokemon->evHP(), pokemon->evATK(), pokemon->evDEF(),
                pokemon->evSPA(), pokemon->evSPD(), pokemon->evSPE()};
            uint16_t stats[] = {pokemon->statHPMax(), pokemon->statATK(), pokemon->statDEF(),
                pokemon->statSPA(), pokemon->statSPD(), pokemon->statSPE()};

            for (int i = 0; i < 6; i++) {
                // Highlight if editing this stat
                if (screen.pokemonDetailsEditing && i == screen.pokemonDetailsSelectedStat) {
                    fb.drawFilledRect(contentX - 5, lineY - 2, contentWidth - 10, lineHeight - 2, Colors::Selected);
                }

                char statLine[128];
                snprintf(statLine, sizeof(statLine), "%s: %03d | %02d  | %03d | %03d",
                        statNames[i], baseStats[i], ivs[i], evs[i], stats[i]);

                Color textColor = (screen.pokemonDetailsEditing && i == screen.pokemonDetailsSelectedStat) ? Colors::Yellow : Colors::Text;
                fb.drawText(contentX, lineY, statLine, textColor);
                lineY += lineHeight;
            }

            // Show totals
            lineY += 5;
            char totalLine[128];
            snprintf(totalLine, sizeof(totalLine), "总和: %03d | %03d | %03d | %03d", baseTotal, ivTotal, evTotal, statTotal);
            fb.drawText(contentX, lineY, totalLine, Colors::TextDim);

        } else { // Other categories
            int lineY = contentY;
            fb.drawText(contentX, lineY, categories[screen.pokemonDetailsCategory], Colors::Text);
            lineY += 40;
            fb.drawText(contentX, lineY, "即将推出...", Colors::TextDim);
            lineY += 30;
            fb.drawText(contentX, lineY, "此类别将在未来更新中实现。", Colors::TextDim);
        }
    }
}
}