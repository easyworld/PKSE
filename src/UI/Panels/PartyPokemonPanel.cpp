#include <string>

#include "UI/Panels/PartyPokemonPanel.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/PokemonTypes.h"
#include "Names/FormNames.h"   // getDisplayName -- variant prefix ("Alolan Raichu", "Combat Breed Tauros")

using namespace Trainer;

namespace UI {
namespace Panels {

    // Draw a type ICON sprite scaled to `h` px tall at (x, y); returns drawn width (0 if none).
    // Named apart from PKSEFramebuffer::drawTypeBadge deliberately -- that one renders a coloured
    // text pill, this one blits the type's artwork. They are different things, not a duplication.
    static int drawTypeIcon(PKSEFramebuffer& fb, Sprite* type, int x, int y, int h) {
        if (!type || !type->data) return 0;
        int w = (type->width * h) / type->height;
        fb.drawImageScaled(x, y, type->width, type->height, w, h, type->data, type->channels);
        return w;
    }

    void drawPartyPokemon(PKSEFramebuffer& fb, const Trainer::Trainer& trainer,
                          int x, int y, int width, int height, int selectedIndex) {
        const auto& party = trainer.party;

        fb.drawCard(x, y, width, height);
        fb.drawText(x + 16, y + 12, "同行宝可梦", Colors::Text, TextStyle::Heading);
        fb.drawHDivider(x + 16, y + 48, width - 32);

        // Two columns of three slots (1-3 left, 4-6 right).
        constexpr int gutter = 16;
        constexpr int slotGap = 12;
        const int gridTop = y + 58;
        const int colW = (width - 3 * gutter) / 2;
        const int colX[2] = { x + gutter, x + gutter + colW + gutter };
        const int slotH = (height - (gridTop - y) - 2 * slotGap - gutter) / 3;

        for (int i = 0; i < 6; i++) {
            const int col = (i >= 3) ? 1 : 0;
            const int rowInCol = (i >= 3) ? (i - 3) : i;
            const int cardX = colX[col];
            const int cardY = gridTop + rowInCol * (slotH + slotGap);
            const bool selected = (selectedIndex >= 0 && i == selectedIndex);

            // Slot card (rounded, HOME-style — matches the cards used everywhere else in the app).
            // A soft shadow + accent border mark the selection, replacing the old square fill and the
            // 4px left edge that didn't sit right against rounded corners.
            constexpr int slotR = 12;
            if (selected) fb.drawSoftShadow(cardX, cardY, colW, slotH, slotR);
            fb.drawFilledRoundedRect(cardX, cardY, colW, slotH, slotR, selected ? Colors::Selected : Colors::PanelAlt);
            fb.drawRoundedRect(cardX, cardY, colW, slotH, slotR, selected ? Colors::Accent : Colors::Border, selected ? 2 : 1);

            constexpr int pad = 14;
            const Pokemon::Pokemon* pokemon = (i < static_cast<int>(party.size())) ? party[i].get() : nullptr;

            if (!pokemon || pokemon->speciesID() == 0) {
                fb.drawText(cardX + pad, cardY + slotH / 2 - 12,
                            "槽位 " + std::to_string(i + 1) + "：空", Colors::TextDim);
                continue;
            }

            const bool isShiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
            const bool av = pokemon->hasAwakeningValues();

            // --- Header row: slot number, species, gender/shiny/partner markers, level ---
            int hx = cardX + pad;
            const int hy = cardY + 9;

            std::string num = std::to_string(i + 1);
            fb.drawText(hx, hy, num, Colors::Accent);
            int numW, numH; fb.measureText(num, numW, numH);
            hx += numW + 10;

            std::string species = Names::getDisplayName(pokemon->speciesID(), pokemon->form(), pokemon->species());
            fb.drawText(hx, hy, species, Colors::Text);
            int spW, spH; fb.measureText(species, spW, spH);
            hx += spW + 6;

            std::string gender = pokemon->genderSymbol();
            if (gender != "?" && gender != "无性别" && gender != "") {
                fb.drawSymbol(hx, hy, gender, (gender == "♂") ? Colors::Blue : Colors::Magenta);
                hx += 20;
            }
            if (isShiny) { fb.drawShinyMark(hx, hy, 16, Colors::ShinyStar); hx += 20; }
            if (trainer.isPartyPokemonStarter(i)) { fb.drawSymbol(hx, hy, "♥", Colors::PartnerHeart); hx += 20; }

            std::string lvl = "等级 " + std::to_string(pokemon->level());
            int lvW, lvH; fb.measureText(lvl, lvW, lvH);
            fb.drawText(cardX + colW - pad - lvW, hy, lvl, Colors::TextDim);

            // --- Stat table: fixed columns for alignment, caption-size data text ---
            struct StatRow { const char* name; int base, iv, evav, stat; };
            const StatRow rows[6] = {
                {"HP",  pokemon->baseHP(),  pokemon->ivHP(),  av ? pokemon->avHP()  : pokemon->evHP(),  pokemon->statHPMax()},
                {"攻击", pokemon->baseATK(), pokemon->ivATK(), av ? pokemon->avATK() : pokemon->evATK(), pokemon->statATK()},
                {"防御", pokemon->baseDEF(), pokemon->ivDEF(), av ? pokemon->avDEF() : pokemon->evDEF(), pokemon->statDEF()},
                {"特攻", pokemon->baseSPA(), pokemon->ivSPA(), av ? pokemon->avSPA() : pokemon->evSPA(), pokemon->statSPA()},
                {"特防", pokemon->baseSPD(), pokemon->ivSPD(), av ? pokemon->avSPD() : pokemon->evSPD(), pokemon->statSPD()},
                {"速度", pokemon->baseSPE(), pokemon->ivSPE(), av ? pokemon->avSPE() : pokemon->evSPE(), pokemon->statSPE()},
            };

            const int labelX = cardX + pad;
            const int cBase  = cardX + 56;
            const int cIV    = cardX + 104;
            const int cEV    = cardX + 144;
            const int cStat  = cardX + 190;
            int tY = cardY + 38;
            constexpr int lineH = 16;

            const Color colHdr = selected ? Colors::Text : Colors::TextDim;
            fb.drawText(cBase, tY, "种族值",           colHdr, TextStyle::Caption);
            fb.drawText(cIV,   tY, "IV",             colHdr, TextStyle::Caption);
            fb.drawText(cEV,   tY, av ? "AV" : "EV", colHdr, TextStyle::Caption);
            fb.drawText(cStat, tY, "能力值",           colHdr, TextStyle::Caption);
            tY += lineH;

            for (const auto& r : rows) {
                fb.drawText(labelX, tY, r.name,                 Colors::TextDim, TextStyle::Caption);
                fb.drawText(cBase,  tY, std::to_string(r.base), Colors::Text,    TextStyle::Caption);
                fb.drawText(cIV,    tY, std::to_string(r.iv),   Colors::Text,    TextStyle::Caption);
                fb.drawText(cEV,    tY, std::to_string(r.evav), Colors::Text,    TextStyle::Caption);
                fb.drawText(cStat,  tY, std::to_string(r.stat), Colors::Accent,  TextStyle::Caption);
                tY += lineH;
            }

            // --- Type badges (stacked) + sprite (right side) ---
            Pokemon::TypePair types = Pokemon::getPokemonTypes(pokemon->speciesID(), pokemon->form());
            const int typeX = cardX + 286;
            drawTypeIcon(fb, SpriteManager::getTypeSprite(types.type1), typeX, cardY + 42, 16);
            if (Pokemon::hasSecondType(types)) {
                drawTypeIcon(fb, SpriteManager::getTypeSprite(types.type2), typeX, cardY + 63, 16);
            }

            Sprite* sprite = SpriteManager::getSprite(pokemon->speciesID(), pokemon->form(), isShiny);
            if (sprite && sprite->data) {
                const int SP = 96;
                int spX = cardX + colW - pad - SP;
                int spY = cardY + 30;
                fb.drawSpriteIdle(spX, spY, SP, SP, sprite->width, sprite->height,
                                  sprite->data, sprite->channels, static_cast<float>(i) * 1.1f);
            }
        }
    }
}
}
