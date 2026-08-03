#include <cstring>
#include <ctime>     // std::time / std::localtime -> a created mon's met date = today
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <sys/stat.h>

#include "Names/ItemPouches.h"   // getPouchItems -> the add-item picker's per-pouch list
#include "Names/MoveInfo.h"      // getMoveBasePP -> a created/picked move gets real PP, not 0 (#F1F2)
#include "Names/MoveNames.h"     // getMoveCount -> scan for a created mon's first legal move
#include "Names/MovePresence.h"  // isMovePresent -> hide moves a game doesn't have (PKHeX-style filter)
#include "Names/LocationNames.h" // getLocationTable -> the Met Location picker's per-game list
#include "Names/FormNames.h"     // getDisplayName -> variant prefix in the "无法存入此游戏" toast
#include "Enums/Ball.h"          // getBallList -> the per-game Ball picker
#include "Legality/Legality.h"   // analyze() -> gate the details-page Legality (R) button when clean

#include "Globals.h"
#include "Save/GetSaveFileContents.h"
#include "UI/TrainerViewScreen.h"
#include "UI/TouchInput.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"
#include "UI/Panels/PartyPokemonPanel.h"
#include "UI/Panels/BoxPokemonPanel.h"
#include "UI/Panels/ItemsPanel.h"
#include "UI/Panels/StoragePanel.h"
#include "UI/Panels/HomeMenuPanel.h"
#include "UI/Dialogs/ItemEditDialog.h"
#include "UI/Dialogs/SaveConfirmDialog.h"
#include "UI/Dialogs/StatEditDialog.h"
#include "UI/Modals/PokemonDetailsModal.h"
#include "Utils/HelperUtilities.h"
#include "Utils/Keyboard.h"
#include "Utils/Logger.h"
#include "Utils/FileUtilities.h"
#include "Utils/Settings.h"
#include "Trainer/Trainer.h"
#include "Trainer/Inventory.h"
#include "Trainer/Inventory9LZA.h"
#include "Trainer/Inventory9SV.h"
#include "Trainer/Inventory8LA.h"
#include "Trainer/Inventory8BDSP.h"
#include "Trainer/Inventory7LGPE.h"
#include "Trainer/Inventory3FRLG.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/Experience.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/LearnsetTable.h"
#include "Conversion/Convert.h"
#include "Utils/StringHelpers.h"

namespace UI {
    static std::string leafName(const std::string& path);   // defined below; used by the ctor's trace

    // UI Layout constants
    constexpr int LEFT_PANEL_X = 12;
    constexpr int LEFT_PANEL_Y = 80;
    constexpr int LEFT_TRAINER_INFO_PANEL_WIDTH = 220;
    constexpr int LEFT_TRAINER_INFO_PANEL_HEIGHT = 210;
    constexpr int LEFT_VIEW_MODE_PANEL_WIDTH = 220;
    constexpr int LEFT_VIEW_MODE_PANEL_HEIGHT = 205;  // fits 4 modes (Party/Boxes/Items/Storage)
    constexpr int LEFT_PANEL_SPACING = 12;
    constexpr int CONTENT_PANEL_X = LEFT_PANEL_X + LEFT_VIEW_MODE_PANEL_WIDTH + 12;
    constexpr int CONTENT_PANEL_Y = LEFT_PANEL_Y;
    constexpr int CONTENT_PANEL_HEIGHT = 560;

    // Creator: build a valid, game-accepted default Pokemon in the current save's format. Species-
    // correct ability / gender / friendship come from the personal table; the user refines the rest
    // in the details editor. See scratchpad/creator_plan.md.
    // Randomize IVs: roll the six IVs to fresh 0-31 values and NOTHING else. Deliberately IVs
    // only -- nature, ability, and everything else are kept, so it can never silently change the
    // mon's identity. (A broader encounter-consistent randomizer -- PID/nature/ability from a real
    // encounter -- is a future version; see docs/FUTURE_VERSIONS.md.)
    static void randomizeIVs(Pokemon::Pokemon* p) {
        if (!p) return;
        for (int i = 0; i < 6; ++i)
            p->setIV(i, static_cast<uint8_t>(Utils::rand32() % 32));
        p->recalculateStats();
        p->refreshChecksum();
    }

    // Origin version byte to stamp on a Pokemon created in this save. A game GROUP deliberately
    // collapses a version pair into one value -- it exists to say "these games share a save format" --
    // so picking the origin from it cannot tell Violet from Scarlet, and a created mon claimed the
    // FIRST game of the pair (Violet -> Scarlet, Shield -> Sword, Shining Pearl -> Brilliant Diamond,
    // Let's Go Eevee -> Let's Go Pikachu, LeafGreen -> FireRed). The title id knows exactly which game
    // is open, and the enum's per-game values ARE the stored origin bytes, so it is used directly; the
    // group only supplies a fallback for an id we don't recognise. Gen 3 stores origin in a 4-bit
    // field, and both its values (FR = 4, LG = 5) fit, so distinguishing them is safe.
    static uint8_t creatorOriginVersion(Trainer::Trainer& tr, u64 titleId) {
        const Enums::GameVersion v = Enums::getGameVersion(titleId);
        if (v != Enums::GameVersion::Invalid && Enums::getGameGroup(v) == tr.getGameGroup())
            return static_cast<uint8_t>(v);
        return Enums::getGroupRepVersion(tr.getGameGroup());
    }

    static std::unique_ptr<Pokemon::Pokemon> buildDefaultMon(Trainer::Trainer& tr, uint16_t species,
                                                            uint8_t version) {
        auto p = tr.createBlankPokemon();
        if (!p) return p;
        const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(species, 0);
        p->setSpecies(species);          // first: drives the EXP growth rate + base-stat lookup
        p->setForm(0);
        p->setEncryptionConstant(Utils::rand32());
        p->setPID(Utils::rand32());
        p->setLevel(1);                  // after species; writes EXP from growth rate + recalcs stats
        // Random nature at birth (the real and stat/mint nature start matched; the mint stays editable).
        { uint8_t nat = static_cast<uint8_t>(Utils::rand32() % 25); p->setNature(nat); p->setStatNature(nat); }
        // Random gender, constrained by the species ratio: fixed for genderless (255) / female-only (254);
        // otherwise rolled against the female threshold (genderRatio 0 -> always male).
        { uint8_t g = (pi.genderRatio == 255) ? 2
                    : (pi.genderRatio == 254) ? 1
                    : (((Utils::rand32() & 0xFF) < pi.genderRatio) ? 1 : 0);
          p->setGender(g); }
        p->setAbility(pi.ability1);      // slot-1 ability id, and mark it slot 1
        p->setAbilityNumber(1);
        p->setFriendship(pi.baseFriendship);
        // Poké Ball -- but Legends: Arceus uses its own ball set, where the Poké Ball is id 28 (the
        // standard Poké Ball id 4 isn't one of its balls and reads as the wrong ball in-game).
        p->setBall(tr.getGameGroup() == Enums::GameVersion::PLA ? 28 : 4);
        p->setLanguage(2);               // English
        p->setOriginGame(version);
        p->setMetLevel(1);
        // Met "here, today": a valid caught date + a real location, so the mon doesn't read as met on
        // 00/00/2000 at location 0 ("无" -- which BDSP renders as "hatched from an egg at Jubilife
        // City"). Date components are years-since-2000 / 1-based month / day; formats without a met date
        // (Gen 3) no-op these setters.
        std::time_t nowT = std::time(nullptr);
        if (const std::tm* lt = std::localtime(&nowT)) {
            p->setMetYear(static_cast<uint8_t>(((lt->tm_year + 1900) - 2000) & 0xFF));
            p->setMetMonth(static_cast<uint8_t>(lt->tm_mon + 1));
            p->setMetDay(static_cast<uint8_t>(lt->tm_mday));
        }
        // A real, recognizable in-game met location for every game group, so a created mon is never met
        // at location 0 / "（无）". Ids are per-game (each game has its own location table + numbering).
        switch (tr.getGameGroup()) {
            case Enums::GameVersion::SV:   p->setMetLocation(8);   break;  // Mesagoza
            case Enums::GameVersion::ZA:   p->setMetLocation(11);  break;  // Centrico Plaza
            case Enums::GameVersion::BDSP: p->setMetLocation(38);  break;  // Oreburgh City
            case Enums::GameVersion::PLA:  p->setMetLocation(7);   break;  // Obsidian Fieldlands
            case Enums::GameVersion::SWSH: p->setMetLocation(12);  break;  // Route 1 (Galar)
            case Enums::GameVersion::GG:   p->setMetLocation(3);   break;  // Route 1 (Kanto)
            case Enums::GameVersion::FRLG: p->setMetLocation(101); break;  // Route 1 (Kanto, Gen 3 numbering)
            default: break;
        }
        // Not from an egg (a caught mon): clear egg origin. The "无蛋获得地点" value is game-specific.
        // BDSP uses Gen 4-style numbering where 0 is a REAL place (Jubilife City) and 65535 is "无"
        // (PKHeX Locations.Default8bNone) -- writing 0 there made a created mon read as "hatched from an
        // Egg" received at Jubilife City. Every other format uses 0 for "无".
        p->setEgg(false);
        p->setEggLocation(tr.getGameGroup() == Enums::GameVersion::BDSP ? 0xFFFF : 0x0000);
        p->setEggYear(0); p->setEggMonth(0); p->setEggDay(0);
        // Clear the HOME ribbon+mark block (0x34-0x45) so a created mon owns no stray ribbon, AND reset
        // AffixedRibbon -- the byte that selects which ribbon the game DISPLAYS. On an all-zero blank it
        // is 0, and ribbon index 0 is the Kalos Champion ribbon, so a fresh mon SHOWED it even with no
        // ribbon owned (the SV report). "无" is 0xFF (-1). Its offset differs per format; a non-zero
        // affix also gates the whole block to the formats that carry the HOME ribbons (not FRLG / GG).
        // Both fields sit in the checksummed region, covered by the refreshChecksum() below.
        {
            size_t affix = 0;
            switch (tr.getGameGroup()) {
                case Enums::GameVersion::SV:
                case Enums::GameVersion::ZA:   affix = 0xD4; break;  // PK9 / PA9
                case Enums::GameVersion::BDSP:
                case Enums::GameVersion::SWSH: affix = 0xE8; break;  // G8PKM (PK8 / PB8)
                case Enums::GameVersion::PLA:  affix = 0xF8; break;  // PA8
                default: break;
            }
            if (affix) {
                auto d = p->getData();
                if (d.size() >= 0x46) std::memset(d.data() + 0x34, 0, 0x46 - 0x34);
                if (d.size() > affix) d.data()[affix] = std::byte{0xFF};   // AffixedRibbon = None
            }
        }
        // First move = the species' first legal move for this game, so the created mon is legal. PLA
        // rejects an unlearnable move as a Bad Egg in-game (Pound isn't a Vulpix move there); the other
        // games merely flag it. Fall back to Pound (id 1) if the learnset table offers nothing.
        uint16_t defMove = 1;
        for (uint16_t m = 1; m < Names::getMoveCount(); ++m)
            if (Pokemon::isLearnable(species, 0, tr.getGameGroup(), m)
                && Names::isMovePresent(m, tr.getGameGroup())) { defMove = m; break; }
        p->setMove(0, defMove);
        p->setMovePP(0, Names::getMoveBasePP(defMove));   // real PP, so it isn't shown at 0 PP (#F1F2)
        p->setMovePPUps(0, 0);
        p->setId32(tr.ID32);
        p->setOTName(Utils::utf8ToUtf16(tr.trainerName));
        p->setOTGender(tr.trainerGender);   // match the trainer -- else Gen 3 reads it as "相遇方式"
        // Default nickname = the species name. The games store the DISPLAY string in the nickname
        // field, so a blank field shows a BLANK name in-game (the created-mon report). isNicknamed is
        // left false -- this is the species default, not a custom name. Gen 3 stores nicknames uppercase.
        {
            std::string spName = p->species();
            if (tr.getGameGroup() == Enums::GameVersion::FRLG)
                for (char& c : spName) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            p->setNickname(Utils::utf8ToUtf16(spName));
        }
        // Let's Go stores + DISPLAYS absolute height/weight (floats at PB7 0x2C / 0xE4); PK8/PK9 keep
        // only the 0-255 scalar and derive the size on the fly. A created LGPE mon left those floats at
        // 0, so it showed 0'00" / 0.0 lbs in-game. Roll random size scalars and compute the absolutes
        // from the species base size -- same formula as the bank convert (PKHeX PB7.Get*Absolute).
        if (tr.getGameGroup() == Enums::GameVersion::GG) {
            auto d = p->getData();
            if (d.size() >= 0xE8) {
                const uint8_t hs = static_cast<uint8_t>(Utils::rand32() & 0xFF);
                const uint8_t ws = static_cast<uint8_t>(Utils::rand32() & 0xFF);
                d.data()[0x3A] = static_cast<std::byte>(hs);
                d.data()[0x3B] = static_cast<std::byte>(ws);
                const float hr = (hs / 255.0f) * 0.79999995f + 0.6f;   // height ratio (-20% .. +40%)
                const float wr = (ws / 255.0f) * 0.40000004f + 0.8f;   // weight ratio (+/- 20%)
                const float hAbs = hr * static_cast<float>(pi.height);
                const float wAbs = hr * wr * static_cast<float>(pi.weight);
                std::memcpy(d.data() + 0x2C, &hAbs, sizeof(float));
                std::memcpy(d.data() + 0xE4, &wAbs, sizeof(float));
            }
        }
        // Roll a random IV spread at birth (same routine as the L-button randomizer, which is unchanged);
        // this recalculates stats and refreshes the checksum, so it stands in for the final pass.
        randomizeIVs(p.get());
        return p;
    }

    // Settings view (the toggle rows), reached from the HOME menu's Settings icon.
    static void drawSettingsView(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int w, int h) {
        fb.drawFilledRoundedRect(x, y, w, h, 16, Colors::Panel);
        fb.drawRoundedRect(x, y, w, h, 16, Colors::Border, 1);
        constexpr int hH = 46;
        fb.drawFilledRoundedRect(x, y, w, hH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + hH - 16, w, 16, Colors::AccentDim);
        fb.drawText(x + 22, y + (hH - fb.lineHeight(TextStyle::Heading)) / 2, "设置", Colors::Text, TextStyle::Heading);

        screen.touchButtons.clear();
        constexpr int kRows = 5;
        // The injection row names its actual scope. It does NOT govern saving a cart-loaded session
        // back to the cart -- that is always allowed. It only unlocks writing an OLDER BACKUP over
        // the live save, which is the case that can roll a game backwards.
        const char* labels[kRows] = { "载入时自动备份", "主题", "允许非法数值",
                                      "Let's Go 招式警告", "允许备份写入游戏存档" };
        std::string values[kRows] = {
            g_autoBackupEnabled ? "开启" : "关闭",
            (g_themeMode == ThemeMode::Dark) ? "深色" : "浅色",
            g_allowIllegalEdits ? "开启" : "关闭",
            g_lgpeMoveWarn ? "开启" : "关闭",
            g_injectToGameSave ? "开启" : "关闭",
        };
        const int rowW = 720, rowH = 64;
        const int rx = x + (w - rowW) / 2;
        int ry = y + hH + 34;
        for (int i = 0; i < kRows; ++i) {
            const bool sel = (screen.settingsSelectedRow == i);
            fb.drawSoftShadow(rx, ry, rowW, rowH, 14);
            fb.drawFilledRoundedRect(rx, ry, rowW, rowH, 14, sel ? Colors::Selected : Colors::PanelAlt);
            if (sel) fb.drawRoundedRect(rx, ry, rowW, rowH, 14, Colors::Accent, 2);
            fb.drawText(rx + 24, ry + (rowH - fb.lineHeight(TextStyle::Body)) / 2, labels[i], Colors::Text, TextStyle::Body);
            int vw, vh; fb.measureText(values[i], vw, vh, TextStyle::Body);
            const int pillW = vw + 48, pillH = 40;
            const int px = rx + rowW - pillW - 20, py = ry + (rowH - pillH) / 2;
            // Amber "开启" for the benign toggles; RED for the two that can damage real data -- the
            // illegal-values override and game-save injection. Colour carries the risk, not just text.
            Color pillFill = Colors::Background, pillText = Colors::Text;
            if (i == 0 && g_autoBackupEnabled)      { pillFill = Colors::Primary;    pillText = Colors::PrimaryText; }
            else if (i == 2 && g_allowIllegalEdits) { pillFill = Color(200, 80, 80); pillText = Colors::White; }
            else if (i == 3 && g_lgpeMoveWarn)      { pillFill = Colors::Primary;    pillText = Colors::PrimaryText; }
            else if (i == 4 && g_injectToGameSave)  { pillFill = Color(200, 80, 80); pillText = Colors::White; }
            fb.drawPill(px, py, pillW, pillH, pillFill);
            fb.drawText(px + (pillW - vw) / 2, py + (pillH - vh) / 2, values[i], pillText, TextStyle::Body);
            screen.touchButtons.push_back({ i, rx, ry, rowW, rowH });
            ry += rowH + 20;
        }
        fb.drawText(rx, ry + 8, "A：切换     B：返回", Colors::TextDim, TextStyle::Caption);
    }

    // Trainer view (HOME-style ID card), reached from the HOME menu's Trainer icon.
    static void drawTrainerView(TrainerViewScreen& screen, PKSEFramebuffer& fb, int x, int y, int w, int h) {
        Trainer::Trainer& t = screen.trainer;
        fb.drawFilledRoundedRect(x, y, w, h, 16, Colors::Panel);
        fb.drawRoundedRect(x, y, w, h, 16, Colors::Border, 1);
        constexpr int hH = 46;
        fb.drawFilledRoundedRect(x, y, w, hH, 16, Colors::AccentDim);
        fb.drawFilledRect(x, y + hH - 16, w, 16, Colors::AccentDim);
        fb.drawText(x + 22, y + (hH - fb.lineHeight(TextStyle::Heading)) / 2, "训练家", Colors::Text, TextStyle::Heading);

        const int cardW = 640, cardX = x + (w - cardW) / 2;
        int cy = y + hH + 34;
        { int nw, nh; fb.measureText(t.trainerName, nw, nh, TextStyle::Title);
          fb.drawText(cardX + (cardW - nw) / 2, cy, t.trainerName, Colors::Text, TextStyle::Title);
          cy += nh + 12; }
        fb.drawHDivider(cardX + 20, cy, cardW - 40);
        cy += 22;

        auto row = [&](const char* label, const std::string& value, Color vc) {
            fb.drawSoftShadow(cardX, cy, cardW, 52, 12);
            fb.drawFilledRoundedRect(cardX, cy, cardW, 52, 12, Colors::PanelAlt);
            fb.drawText(cardX + 24, cy + (52 - fb.lineHeight(TextStyle::Body)) / 2, label, Colors::TextDim, TextStyle::Body);
            int vw, vh; fb.measureText(value, vw, vh, TextStyle::Body);
            fb.drawText(cardX + cardW - 24 - vw, cy + (52 - vh) / 2, value, vc, TextStyle::Body);
            cy += 62;
        };
        row("训练家 ID", std::to_string(t.TID16) + " / " + std::to_string(t.SID16), Colors::Text);
        row("完整 TID", std::to_string(t.TID), Colors::Text);
        row("完整 SID", std::to_string(t.SID), Colors::Text);
        row("金钱", "$" + std::to_string(t.money), Colors::Primary);
    }

    TrainerViewScreen::TrainerViewScreen(Trainer::Trainer& trainer, const std::string& titleName, const std::string& backupDir, u64 titleId, AccountUid userUid, bool loadedFromCart)
        : trainer(trainer), titleName(titleName), backupDir(backupDir), gameVersion(Utils::getTitleVersion(titleId)), titleId(titleId), userUid(userUid) {
        // Assigned in the body rather than the init list: it is declared far below these members, and
        // C++ initialises in DECLARATION order, so listing it here would only earn a -Wreorder.
        this->loadedFromCart = loadedFromCart;
        saveDestIndex = defaultSaveDest();

        // Open on the box the game was last left on (persisted per-game as the "当前盒子"),
        // so the editor lands where the player was. Clamp in case a save holds a stale index.
        {
            const uint8_t cb = trainer.getCurrentBox();
            if (cb < trainer.getBoxCount()) { selectedBoxIndex = static_cast<int>(cb); stSaveBox = static_cast<int>(cb); }
        }

        // Persistent cross-game bank for the Storage view (unified; loads any existing on-SD contents).
        bank = std::make_unique<Trainer::Bank>();
        // A damaged bank file drops slots silently otherwise -- the user would just find Pokemon
        // missing with no explanation.
        if (bank && bank->lastLoadRejects() > 0) {
            postStatus(std::to_string(bank->lastLoadRejects()) +
                       " 个损坏的银行槽位无法读取，已跳过。", 480);
        }

        // Open the test trace with everything needed to interpret the rest of the run: which build,
        // which game, where the save came from, and the state of every setting that changes
        // behaviour. Without this a trace is a list of actions with no way to judge them.
        Utils::logTestSession(
            "pkse=" + VERSION_STRING +
            " game=\"" + titleName + "\" gamever=" + (gameVersion.empty() ? "?" : gameVersion) +
            " src=" + (this->loadedFromCart ? "CART" : "BACKUP") +
            " backup=\"" + leafName(backupDir) + "\"" +
            " rev=\"" + (trainer.saveRevisionString.empty() ? "Base" : trainer.saveRevisionString) + "\"" +
            " party=" + std::to_string(trainer.getPartySize()) +
            " boxes=" + std::to_string(trainer.getBoxCount()) +
            " inject=" + (g_injectToGameSave ? "ON" : "OFF") +
            " illegal=" + (g_allowIllegalEdits ? "ON" : "OFF") +
            " autobackup=" + (g_autoBackupEnabled ? "ON" : "OFF") +
            " lgpewarn=" + (g_lgpeMoveWarn ? "ON" : "OFF") +
            " theme=" + (g_themeMode == ThemeMode::Dark ? "dark" : "light"));
        if (bank) {
            Utils::logTest("BANKLOAD rejects=" + std::to_string(bank->lastLoadRejects()));
        }
    }

    void TrainerViewScreen::returnHeldToOrigin() {
        if (!heldPokemon) return;
        auto place = [&](int pane, int box, int slot) -> bool {
            if (pane == 1 && !bank) return false;
            auto& dst = (pane == 0) ? trainer.boxes[box][slot] : bank->boxes[box][slot];
            if (!dst || dst->speciesID() == 0) { dst = std::move(heldPokemon); return true; }
            return false;
        };
        // Prefer the exact origin; if it's since been filled, fall back to any empty slot in the
        // origin pane so a carried Pokemon can never be dropped/lost.
        if (place(heldPane, heldFromBox, heldFromSlot)) return;
        const int slots = (heldPane == 0) ? static_cast<int>(trainer.getSlotsPerBox())
                                          : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX);
        for (int s = 0; s < slots; ++s) if (place(heldPane, heldFromBox, s)) return;
        const int boxCount = (heldPane == 0) ? static_cast<int>(trainer.getBoxCount())
                                             : static_cast<int>(Trainer::Bank::BANK_BOX_COUNT);
        for (int b = 0; b < boxCount; ++b)
            for (int s = 0; s < slots; ++s)
                if (place(heldPane, b, s)) return;
    }

    // Reference to a storage slot's unique_ptr (pane 0 = save boxes, 1 = bank). Callers must
    // ensure `bank` exists for pane 1.
    std::unique_ptr<Pokemon::Pokemon>& TrainerViewScreen::storageSlot(int pane, int box, int slot) {
        return pane == 0 ? trainer.boxes[box][slot] : bank->boxes[box][slot];
    }

    // A save-pane slot that is a party member (LGPE) is locked: removing/displacing it would
    // orphan the party pointer, and LGPE keeps a separate party copy that wins on save. No-op for
    // SWSH/LZA (party is a separate structure -> getPartyPosition returns 0).
    bool TrainerViewScreen::storageSlotLocked(int pane, int box, int slot) {
        return pane == 0 && trainer.getPartyPosition(box, slot) > 0;
    }

    // The tail of the + handler: prompt about unsaved GAME-save changes, else leave. Split out so the
    // bank's Save/Discard prompt can resume the exit once the user has answered it.
    void TrainerViewScreen::beginAppExit() {
        if (hasUnsavedChanges && !saveConfirmActive) {
            exitingWithUnsavedChanges = true;
            exitingViaPlus = true;   // remember we're exiting via the + button
            saveConfirmActive = true;
            return;
        }
        exitRequested = true;
    }

    // Convert `pk` in place into the format needed to live in `destPane`. The bank (pane 1) accepts
    // anything as-is; a save slot (pane 0) requires the open game's format, so a foreign mon is CONVERTED
    // in place if a supported route exists (M5 Phase B). Returns false + posts a status when it can't go
    // there (out-of-dex / unsupported gen pair). Shared by the single-mon and bulk placement paths.
    bool TrainerViewScreen::convertForPane(std::unique_ptr<Pokemon::Pokemon>& pk, int destPane) {
        if (destPane != 0 || !pk) return true;                          // bank: store as-is
        if (pk->getGameGroup() == trainer.getGameGroup()) {
            // Same game, so no conversion -- but re-checksum before it enters the save. The
            // bank stores native bytes untouched by design, so this SHOULD be a no-op writing back
            // the identical value; that is exactly why it is cheap insurance. A stale or damaged
            // checksum reaching the game's box writer surfaces in-game as a Bad Egg.
            // (Only on the way INTO a save. Deposits return above, so a banked mon is never
            //  mutated -- see Appendix C.)
            pk->refreshChecksum();
            return true;
        }
        Conversion::Result res;
        const std::string species = pk->species();
        auto converted = Conversion::convert(*pk, trainer.getGameGroup(), res);
        if (converted) {
            // Cross-gen conversion is where the subtle transfer bugs have historically lived
            // (fainted arrivals, deleted moves, garbage levels), so every one gets a line.
            Utils::logTest("CONVERT  species=\"" + species + "\" -> " +
                           std::to_string(static_cast<int>(trainer.getGameGroup())) +
                           " lvl=" + std::to_string(converted->level()) +
                           " hp=" + std::to_string(converted->statHPMax()) + " result=OK");
            pk = std::move(converted); return true;      // convert in place
        }
        Utils::logTest("CONVERT  species=\"" + species + "\" result=REFUSED msg=\"" +
                       Conversion::resultMessage(res) + "\"");
        storageStatus = Conversion::resultMessage(res);
        storageStatusFrames = 150;   // ~2.5s at 60fps
        return false;
    }

    // Prepare the carried (single) mon for placement into `pane`. Thin wrapper over convertForPane.
    bool TrainerViewScreen::prepareHeldForPane(int pane) {
        return convertForPane(heldPokemon, pane);
    }

    // True if placing `pk` into `pane` would run a Let's Go conversion (exactly one side is LGPE), which
    // resets AVs/EVs -> the user is asked to acknowledge it. Only save-pane (0) placements convert; the
    // bank (1) stores native bytes, so a deposit never resets anything.
    bool TrainerViewScreen::lgpeConversionInvolved(int pane, const Pokemon::Pokemon* pk) const {
        if (pane != 0 || !pk) return false;
        const bool srcGG = pk->getGameGroup() == Enums::GameVersion::GG;
        const bool dstGG = trainer.getGameGroup() == Enums::GameVersion::GG;
        return srcGG != dstGG;
    }

    // True if any mon in the current multi-selection would run an LGPE conversion when dropped into destPane.
    bool TrainerViewScreen::selectionInvolvesLgpe(int destPane) const {
        if (destPane != 0 || !bank) return false;
        for (const auto& r : multiSel) {
            const auto& p = (r.pane == 0) ? trainer.boxes[r.box][r.slot] : bank->boxes[r.box][r.slot];
            if (p && lgpeConversionInvolved(destPane, p.get())) return true;
        }
        return false;
    }

    // True if placing `pk` into `pane` converts a non-Gen3 mon DOWN into Gen 3 (FR/LG). That path rebuilds
    // the PID to preserve the nature (Gen 3 derives nature FROM the PID), which is destructive and can read
    // as illegal -- so it is always confirmed, independent of the LGPE-warn setting. Only save-pane (0)
    // placements convert; the bank stores native bytes.
    bool TrainerViewScreen::gen3DowngradeInvolved(int pane, const Pokemon::Pokemon* pk) const {
        if (pane != 0 || !pk) return false;
        return trainer.getGameGroup() == Enums::GameVersion::FRLG
            && pk->getGameGroup() != Enums::GameVersion::FRLG;
    }

    // True if any mon in the current multi-selection would run a Gen 3 downgrade when dropped into destPane.
    bool TrainerViewScreen::selectionInvolvesGen3Downgrade(int destPane) const {
        if (destPane != 0 || !bank || trainer.getGameGroup() != Enums::GameVersion::FRLG) return false;
        for (const auto& r : multiSel) {
            const auto& p = (r.pane == 0) ? trainer.boxes[r.box][r.slot] : bank->boxes[r.box][r.slot];
            if (p && p->getGameGroup() != Enums::GameVersion::FRLG) return true;
        }
        return false;
    }

    // build the Ability picker's reordered option list -- the species/form's legal abilities
    // first (deduped; these render green + sit at the top), then every remaining ability id. Sets
    // pickerOrder + pickerLegalCount, and pickerSel to the row that shows `current`.
    void TrainerViewScreen::buildAbilityPickerOrder(uint16_t species, uint8_t form, uint16_t current) {
        pickerOrder.clear();
        const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(species, form);
        const int legal[3] = { pi.ability1, pi.ability2, pi.abilityHidden };
        for (int a : legal) {
            bool dup = false;
            for (int x : pickerOrder) if (x == a) { dup = true; break; }
            if (!dup) pickerOrder.push_back(a);
        }
        pickerLegalCount = static_cast<int>(pickerOrder.size());
        const int total = Dialogs::pickerOptionCount(Dialogs::PickerKind::Ability);
        for (int a = 0; a < total; ++a) {
            bool isLegal = false;
            for (int i = 0; i < pickerLegalCount; ++i) if (pickerOrder[i] == a) { isLegal = true; break; }
            if (!isLegal) pickerOrder.push_back(a);
        }
        pickerSel = 0;
        for (int i = 0; i < static_cast<int>(pickerOrder.size()); ++i)
            if (pickerOrder[i] == static_cast<int>(current)) { pickerSel = i; break; }
    }

    // Creator: fill the species picker with only the species obtainable in the open game (via the
    // personal presence bitmask), unless "允许非法数值" is on (then every species is offered).
    // Reuses pickerOrder (row -> species id); pickerLegalCount stays 0 (no green highlight for species).
    void TrainerViewScreen::buildCreatorSpeciesOrder() {
        pickerOrder.clear();
        pickerLegalCount = 0;
        uint8_t bit = 0;
        switch (trainer.getGameGroup()) {
            case Enums::GameVersion::GG:   bit = Pokemon::PERSONAL_GAME_GG;   break;
            case Enums::GameVersion::SWSH: bit = Pokemon::PERSONAL_GAME_SWSH; break;
            case Enums::GameVersion::BDSP: bit = Pokemon::PERSONAL_GAME_BDSP; break;
            case Enums::GameVersion::PLA:  bit = Pokemon::PERSONAL_GAME_PLA;  break;
            case Enums::GameVersion::SV:   bit = Pokemon::PERSONAL_GAME_SV;   break;
            case Enums::GameVersion::ZA:   bit = Pokemon::PERSONAL_GAME_ZA;   break;
            default: break;
        }
        const int total = Dialogs::pickerOptionCount(Dialogs::PickerKind::Species);
        // FireRed/LeafGreen (Gen 3) isn't in the presence bitmask, so its bit is 0 -- filtering by it
        // would leave the Select Species list EMPTY. Offer the Gen 3 National Dex (1-386) instead.
        const bool isFRLG = (trainer.getGameGroup() == Enums::GameVersion::FRLG);
        for (int s = 1; s < total; ++s) {  // skip 0 = None
            bool ok;
            if (g_allowIllegalEdits) ok = true;
            else if (isFRLG)         ok = (s <= 386);
            else                     ok = (Pokemon::getPersonalInfo(static_cast<uint16_t>(s), 0).presence & bit) != 0;
            if (ok) pickerOrder.push_back(s);
        }
        pickerSel = 0;
    }

    // (move half): fill the move picker with the mon's LEARNABLE moves first (green + top), then
    // every other move. Learnability is the per-game single-stage pool from the learnset table.
    void TrainerViewScreen::buildMovePickerOrder(uint16_t species, uint8_t form, Enums::GameVersion group, uint16_t current) {
        pickerOrder.clear();
        const int total = Dialogs::pickerOptionCount(Dialogs::PickerKind::Move);
        std::vector<bool> legal(total, false);
        for (int m = 1; m < total; ++m)
            if (Pokemon::isLearnable(species, form, group, static_cast<uint16_t>(m))
                && Names::isMovePresent(static_cast<uint16_t>(m), group)) {
                legal[m] = true; pickerOrder.push_back(m);
            }
        pickerLegalCount = static_cast<int>(pickerOrder.size());
        // Then the present-but-illegal moves (not green): still selectable, flagged only by the legality
        // check -- PKHeX-style. Moves that DON'T EXIST in this game (dummied / out of range, e.g. Pound in
        // Legends: Arceus, which the game turns into a Bad Egg) are dropped, matching PKHeX's editor.
        pickerOrder.push_back(0);   // None first in the non-legal section, to clear a slot
        for (int m = 1; m < total; ++m)
            if (!legal[m] && Names::isMovePresent(static_cast<uint16_t>(m), group)) pickerOrder.push_back(m);
        pickerSel = 0;
        for (int i = 0; i < static_cast<int>(pickerOrder.size()); ++i)
            if (pickerOrder[i] == static_cast<int>(current)) { pickerSel = i; break; }
    }

    // Resolve the Pokemon the details editor is currently targeting.
    Pokemon::Pokemon* TrainerViewScreen::detailsTargetPokemon() {
        switch (details.source) {
            case EditSource::Party:
                if (details.partyIndex >= 0 && details.partyIndex < static_cast<int>(trainer.party.size()))
                    return trainer.party[details.partyIndex].get();
                return nullptr;
            case EditSource::Bank:
                if (bank && details.bankBox >= 0 && details.bankBox < static_cast<int>(bank->boxes.size()) &&
                    details.bankSlot >= 0 && details.bankSlot < static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX))
                    return bank->boxes[details.bankBox][details.bankSlot].get();
                return nullptr;
            case EditSource::Box:
            default:
                if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                    selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS))
                    return trainer.boxes[selectedBoxIndex][selectedItemIndex].get();
                return nullptr;
        }
    }

    // After editing detailsTargetPokemon(), keep an LGPE party member's two representations (its box
    // slot + its independent party copy) in sync. Otherwise the save's party overlay clobbers a box
    // edit (or the box display goes stale after a party edit). No-op for gens without that duplication.
    void TrainerViewScreen::mirrorEditedPartyMember() {
        if (details.source == EditSource::Box) {
            trainer.mirrorPartyMemberFromBox(static_cast<size_t>(selectedBoxIndex), static_cast<size_t>(selectedItemIndex));
        } else if (details.source == EditSource::Party) {
            trainer.mirrorPartyMemberFromParty(static_cast<size_t>(details.partyIndex));
        }
        // EditSource::Bank has no party duplication.
    }

    // ---- Details-modal edit baseline / "未保存的更改" marker ------------------------------------
    // Modal edits mutate the live mon in place. We snapshot the target's decrypted bytes on open;
    // pokemonEditDirty() compares against it to drive the top-bar "未保存的更改" marker. X (Save)
    // re-snapshots (commit -> marker clears). Closing the page WITHOUT Save calls restoreEditTarget(),
    // which rolls the mon back to the snapshot, so unsaved IV/EV/etc. edits are DISCARDED -- the
    // individual Save button is the only commit point.

    void TrainerViewScreen::snapshotEditTarget() {
        details.editSnapshot.clear();
        if (const Pokemon::Pokemon* t = detailsTargetPokemon()) {
            const auto d = t->getData();
            details.editSnapshot.assign(d.begin(), d.end());
        }
    }

    bool TrainerViewScreen::pokemonEditDirty() {
        if (details.editSnapshot.empty()) return false;
        const Pokemon::Pokemon* t = detailsTargetPokemon();
        if (!t) return false;
        const auto d = t->getData();
        return d.size() != details.editSnapshot.size()
            || !std::equal(d.begin(), d.end(), details.editSnapshot.begin());
    }

    // Roll the details target back to the snapshot: copy the baseline bytes over the live mon, then
    // re-mirror so an LGPE box/party twin reverts too. Because edits mutate the live buffer in place,
    // undoing them just means restoring the captured bytes (checksum + stats included -- the snapshot
    // is the whole serialized record). The snapshot is re-taken on X = Save, so this discards only the
    // edits made SINCE the last save. Called on close-without-Save; a no-op when nothing changed.
    void TrainerViewScreen::restoreEditTarget() {
        if (details.editSnapshot.empty()) return;
        Pokemon::Pokemon* t = detailsTargetPokemon();
        if (!t) return;
        auto d = t->getData();
        if (d.size() != details.editSnapshot.size()) return;   // fixed PK buffer -> always equal; guard anyway
        std::copy(details.editSnapshot.begin(), details.editSnapshot.end(), d.begin());
        mirrorEditedPartyMember();
    }

    void TrainerViewScreen::closeDetailsModal() {
        details.active = false;
        details.source = EditSource::Box;
        details.selectedField = 0;
        details.legalityOverlay = false;
        details.ribbonOverlay = false;
        details.editSnapshot.clear();
    }

    // The bag keeps "已有但为空" slots (count 0 — e.g. used-up story key items) that we must
    // preserve on save (PKHeX keeps them positionally), but they shouldn't clutter the UI. This
    // returns the raw indices of the current pouch's items worth showing/editing (count > 0), and
    // is the single source of truth so the panel, navigation, and edit all stay in lockstep.
    // Returns the id of the touch button under a fresh tap this frame, or -1 if none. Buttons are
    // captured during the previous frame's draw (only the active overlay populates touchButtons).
    int TrainerViewScreen::touchedButtonId(const TouchInput& touch) const {
        if (!touch.justPressed()) return -1;
        for (const auto& b : touchButtons) {
            if (touch.x() >= b.x && touch.x() < b.x + b.w &&
                touch.y() >= b.y && touch.y() < b.y + b.h)
                return b.id;
        }
        return -1;
    }

    // Rename the focused box via the Switch keyboard.
    //
    // Safe to call from update(): the UI loop is update() -> draw() -> flush(), so no NanoVG frame
    // is open when swkbd suspends the app. Calling this from draw() would strand a half-built frame.
    void TrainerViewScreen::renameBox(int boxIndex) {
        const size_t maxChars = trainer.getMaxBoxNameLength();
        if (maxChars == 0) return;
        if (boxIndex < 0 || boxIndex >= static_cast<int>(trainer.boxNames.size())) return;

        const std::string current = trainer.boxNames[boxIndex];
        const Utils::KeyboardResult res =
            Utils::promptText("重命名盒子", "盒子名称", current, static_cast<int>(maxChars));
        if (!res.accepted) return;          // cancel means "保持不变", NOT "清除"
        if (res.text == current) return;

        // Refuse rather than mangle: Gen 3 can only store ~70 glyphs, so a name the keyboard was
        // happy to produce may be unwritable there.
        if (!trainer.canStoreBoxName(res.text)) {
            Utils::logTest("BOXNAME  box=" + std::to_string(boxIndex + 1) +
                           " new=\"" + res.text + "\" result=REFUSED_CHARSET");
            storageStatus = "此游戏无法保存这些字符（仅支持 A-Z、0-9 和 ! ? . -）。";
            storageStatusFrames = 240;
            return;
        }

        // An empty name is legitimate: the games treat a blank as "使用默认值", and
        // parseBoxNames already turns a blank back into "盒子 N" on reload.
        Utils::logTest("BOXNAME  box=" + std::to_string(boxIndex + 1) +
                       " old=\"" + current + "\" new=\"" + res.text + "\" result=OK");
        trainer.boxNames[boxIndex] = res.text;
        // Only a box marked here is written on save. boxNames also holds display defaults for boxes
        // the save leaves unnamed, and persisting those would invent names the player never set.
        trainer.markBoxNameDirty(static_cast<size_t>(boxIndex));
        hasUnsavedChanges = true;
        storageStatus = res.text.empty() ? "盒子名称已清除。" : ("盒子已重命名为“" + res.text + "”。");
        storageStatusFrames = 180;
    }

    // Rename a BANK box. Unlike save boxes, bank names are PKSE-internal (stored UTF-8 in bank.dat,
    // no game charset limit), and the default label is "银行箱 N" rather than a stored string -- so an
    // empty name means "使用默认值", and the keyboard starts from the raw stored name (blank by
    // default) rather than from the "银行箱 N" label.
    void TrainerViewScreen::renameBankBox(int box) {
        if (!bank || box < 0 || box >= static_cast<int>(Trainer::Bank::BANK_BOX_COUNT)) return;
        const std::string current = bank->boxNames[box];
        const Utils::KeyboardResult res =
            Utils::promptText("重命名银行箱", "银行箱名称", current,
                              static_cast<int>(Trainer::Bank::MAX_BOX_NAME_LEN));
        if (!res.accepted) return;              // cancel = leave it alone
        if (res.text == current) return;

        bank->boxNames[box] = res.text;
        // A bank rename is an unsaved BANK change: it feeds serialize()/hasChanged(), so leaving the
        // storage view now raises the Save/Discard prompt just like a deposit does. (Not tied to the
        // trainer's hasUnsavedChanges, which is the save file's dirty flag.)
        Utils::logTest("BANKBOXNAME  box=" + std::to_string(box + 1) +
                       " new=\"" + res.text + "\" result=OK");
        storageStatus = res.text.empty() ? "银行箱名称已恢复默认。"
                                         : ("银行箱已重命名为“" + res.text + "”。");
        storageStatusFrames = 180;
    }

    // A backup's leaf folder name IS its name everywhere in the UI (PKSM does the same), so this is
    // what the user recognises when we report where a save went.
    static std::string leafName(const std::string& path) {
        const size_t slash = path.find_last_of('/');
        return (slash == std::string::npos) ? path : path.substr(slash + 1);
    }

    void TrainerViewScreen::performSave(const std::string& destDir, bool injectToTitle) {
        // The game's "当前盒子" is kept in sync live while a box view is open (see update()), so
        // it already reflects wherever the user last was -- including a box change made in Storage
        // before backing out (Storage's own X sorts; this game save happens later). Nothing to do here.
        const bool ok = Save::saveTrainerInfo(trainer, destDir.c_str(), titleId, userUid, injectToTitle);
        saveConfirmActive = false;

        // The single most important trace line: what was written, where, and whether it worked.
        Utils::logTest(std::string("SAVE     dest=") + (injectToTitle ? "GAME" : "BACKUP") +
                       " folder=\"" + leafName(destDir) + "\"" +
                       " inject=" + (injectToTitle ? "1" : "0") +
                       " illegaldata=" + (illegalDataWritten ? "1" : "0") +
                       " result=" + (ok ? "OK" : "FAILED"));

        if (ok) {
            hasUnsavedChanges = false;
            statEdit.dialogActive = false;
            details.active = false;
            details.editing = false;
            creator.editing = false; creator.keepConfirmActive = false;   // committed by the save
            // Name the destination back to the user: with three of them, "已保存" alone is ambiguous
            // and the whole point of the picker is knowing WHERE it went.
            postStatus(injectToTitle ? "已写入游戏存档。"
                                     : ("已写入备份“" + leafName(destDir) + "”。"), 200);
            // A new backup becomes the session's working copy, so a second save goes to the same
            // place instead of silently forking another folder off the original.
            backupDir = destDir;
        } else {
            // A failed save used to be INDISTINGUISHABLE from a successful one.
            postStatus("保存失败：更改仍未保存，未写入任何内容。", 480);
            hasUnsavedChanges = true;
        }
    }

    // Create a new named backup folder, seeded with a copy of the one currently open.
    //
    // The copy matters: several games' save paths RE-READ the destination's existing save file to
    // recover the blocks they don't touch (Let's Go most obviously), so writing into an empty
    // directory would produce a truncated save. "新备份" therefore means "a copy of this save,
    // plus my edits" -- which is also what the user means by it.
    std::string TrainerViewScreen::createNamedBackupDir(const std::string& name) {
        // Never let a typed name reach the filesystem unfiltered: a '/' or ".." would escape the
        // PKSE directory entirely. Keep it to characters that are safe on FAT32 as well.
        std::string leaf;
        for (const char c : name) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_')
                leaf += c;
        }
        while (!leaf.empty() && leaf.back() == ' ') leaf.pop_back();     // FAT32 dislikes trailing spaces
        while (!leaf.empty() && leaf.front() == ' ') leaf.erase(leaf.begin());
        if (leaf.empty()) return "";

        const std::string gameDir = BASE_SAVE_DIRECTORY + "/" + titleName;
        auto exists = [](const std::string& p) { struct stat st{}; return stat(p.c_str(), &st) == 0; };

        // Suffix rather than overwrite -- silently replacing a backup the user made earlier would be
        // the worst possible reading of "创建新存档".
        std::string unique = leaf;
        for (int n = 2; n < 1000 && exists(gameDir + "/" + unique); ++n)
            unique = leaf + "-" + std::to_string(n);

        const std::string destDir = gameDir + "/" + unique;
        if (mkdir(destDir.c_str(), 0777) != 0 && errno != EEXIST) {
            Utils::logErrorToFile("Failed to create named backup directory", destDir.c_str());
            return "";
        }
        if (!Utils::copyDirectory(backupDir.c_str(), destDir.c_str())) {
            Utils::logErrorToFile("Failed to seed named backup from", backupDir.c_str());
            return "";
        }
        return destDir;
    }

    std::vector<int> TrainerViewScreen::visibleItemIndices() const {
        std::vector<int> vis;
        if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
            const auto& pouch = trainer.items[selectedCategory];
            vis.reserve(pouch.size());
            for (int i = 0; i < static_cast<int>(pouch.size()); ++i) {
                if (pouch[i].count > 0) vis.push_back(i);
            }
        }
        return vis;
    }

    // How many item slots the current pouch can hold, for the add-item flow.
    //
    // Two storage models. The id-indexed games (BDSP / S-V / Z-A) build their pouch vector by
    // walking every legal id, so an entry already exists for anything addable and nothing is ever
    // appended -- they report a bound large enough to never block. The slot-based games store only
    // what the bag holds, so a new item really does append and must respect the pouch's capacity.
    // Legends: Arceus reports 0 (appending unsupported): its PouchInfo8LA carries no capacity and
    // its item block is resized on write, so appending without a documented bound could overflow it.
    int TrainerViewScreen::currentPouchCapacity() const {
        using namespace Trainer;
        const int c = selectedCategory;
        if (c < 0) return 0;
        switch (trainer.getGameGroup()) {
            case Enums::GameVersion::FRLG:
                return (c < static_cast<int>(POUCH_COUNT3_FRLG))
                     ? getPouchInfo3FRLG(static_cast<PouchType3FRLG>(c)).maxSlots : 0;
            case Enums::GameVersion::GG:
                return (c < static_cast<int>(PouchType7LGPE::Count))
                     ? getPouchInfo7LGPE(static_cast<PouchType7LGPE>(c)).maxSlots : 0;
            case Enums::GameVersion::SWSH:
                return (c < static_cast<int>(PouchType8SWSH::Count))
                     ? getPouchInfo8SWSH(static_cast<PouchType8SWSH>(c)).maxCount : 0;
            case Enums::GameVersion::PLA:
                // Fixed-capacity packed pouches; the general bag grows with Satchel Upgrades.
                return static_cast<int>(trainer.getItemPouchCapacity(c));
            default:
                return 1 << 20;   // id-indexed: the entry is already there, so this never binds
        }
    }

    // Largest stack the current pouch allows for one item.
    //
    // This is a SAVE-SAFETY limit, not cosmetics. Writing a count the game never produces can
    // overflow the pouch and corrupt whatever follows it -- in Gen 3 that means spilling into the
    // KEY ITEMS pocket, which the user has hit for real. The editor previously offered 999 for
    // every game and every pouch, which is wrong in both directions.
    //
    // Values are PKHeX's per-pouch InventoryPouch maxima (PlayerBag*.cs / ItemStorage*.GetMax):
    //   * KEY ITEMS are 1 in EVERY game -- they are possession flags, not stacks.
    //   * Let's Go caps TMs at 1, and Z-A caps TMs AND Mega Stones at 1.
    //
    // Gen 3 uses PKHeX's 999. This was investigated properly and is SETTLED -- don't re-tighten it:
    //   * A stricter 99 was first considered on a report of an oversized stack overflowing a bag
    //     into the key-items pocket. That overflow is real, but it is **Gen 2 (Crystal)**, whose
    //     bag caps stacks at 99 and spills into fresh slots -- not Gen 3's model.
    //   * PKHeX applies no Gen 3 special-casing anywhere: PlayerBag3FRLG declares 999 like every
    //     other game, and InventoryPouch3 adds no count logic at all.
    //   * PKSM on real Gen 3 hardware permits the full u16 (65535), confirming Gen 3 does not
    //     enforce a low cap. We stay at PKHeX's 999 rather than matching PKSM: it is the
    //     legality-aware bound, and nothing is gained by allowing counts no game will ever show.
    int TrainerViewScreen::currentItemMaxCount() const {
        using GV = Enums::GameVersion;
        const int c = selectedCategory;
        switch (trainer.getGameGroup()) {
            case GV::FRLG: return (c == static_cast<int>(Trainer::PouchType3FRLG::KeyItems)) ? 1 : 999;
            // Let's Go caps TMs at 1. Its "重要道具" pouch is NOT key-items-only -- it is PKHeX's
            // Items pouch with regular and key items MIXED (max 999), so it must not be capped at 1.
            case GV::GG:   return (c == static_cast<int>(Trainer::PouchType7LGPE::TMs)) ? 1 : 999;
            case GV::SWSH: return (c == static_cast<int>(Trainer::PouchType8SWSH::KeyItems)) ? 1 : 999;
            // PLA caps Key Items AND Recipes at 1 (possession flags), like PKHeX PlayerBag8a.
            case GV::PLA:  return (c == static_cast<int>(Trainer::PouchType8LA::KeyItems)
                                || c == static_cast<int>(Trainer::PouchType8LA::Recipes)) ? 1 : 999;
            case GV::BDSP: return (c == static_cast<int>(Trainer::PouchType8BDSP::KeyItems)) ? 1 : 999;
            case GV::SV:   return (c == static_cast<int>(Trainer::PouchType9SV::KeyItems)) ? 1 : 999;
            case GV::ZA:   return (c == static_cast<int>(Trainer::PouchType9LZA::KeyItems)
                                || c == static_cast<int>(Trainer::PouchType9LZA::TMs)
                                || c == static_cast<int>(Trainer::PouchType9LZA::MegaStones)) ? 1 : 999;
            default:       return 999;
        }
    }

    // Open the details modal on a storage slot (reuses the Box path for the save pane).
    void TrainerViewScreen::openStorageEditor(int pane, int box, int slot) {
        if (pane == 1) {
            details.source = EditSource::Bank;
            details.bankBox = box;
            details.bankSlot = slot;
        } else {
            details.source = EditSource::Box;
            selectedBoxIndex = box;
            selectedItemIndex = slot;
        }
        details.active = true;
        details.leftScroll = 0;   // start the info column at the top
        details.editing = false;
        details.category = 0;
        details.selectedStat = 0;
        details.selectedField = 0;
        details.hexMode = 0;
        creator.editing = false;  // default; the creator flow sets it true right after this call
        snapshotEditTarget();    // dirty-check baseline (unused for the creator: it has Keep/Discard)
    }

    // Green "招式": bulk-move the multi-selection into the opposite pane, filling empty slots from
    // that pane's current box forward. Convertible mons move; any that can't convert into the
    // destination game or don't fit stay selected (D5), and the user is told what was left and why.
    void TrainerViewScreen::transferSelectionToOtherPane() {
        if (multiSel.empty() || !bank) return;
        const int from = multiSel.front().pane;
        const int to = 1 - from;
        const int dSlots = (to == 0) ? static_cast<int>(trainer.getSlotsPerBox()) : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX);
        const int dBoxes = (to == 0) ? static_cast<int>(trainer.getBoxCount()) : static_cast<int>(Trainer::Bank::BANK_BOX_COUNT);
        int b = (to == 0) ? stSaveBox : stBankBox;
        int s = 0;
        std::vector<SlotRef> leftover;
        int moved = 0, blocked = 0, nofit = 0;
        std::string firstName;
        const char* firstWhy = nullptr;
        for (const auto& src : multiSel) {
            auto& srcPtr = storageSlot(src.pane, src.box, src.slot);
            if (!srcPtr) continue;
            // Cross-game into a save: skip (keep selected) a foreign mon with no conversion route, and
            // move the rest -- don't strand the whole group behind one blocker (D5).
            if (to == 0 && srcPtr->getGameGroup() != trainer.getGameGroup()) {
                Conversion::Result res;
                if (!Conversion::canConvert(*srcPtr, trainer.getGameGroup(), res)) {
                    ++blocked;
                    if (!firstWhy) {
                        firstName = Names::getDisplayName(srcPtr->speciesID(), srcPtr->form(),
                                                          Trainer::getSpeciesName(srcPtr->speciesID()));
                        firstWhy  = Conversion::resultMessage(res);
                    }
                    leftover.push_back(src);
                    continue;
                }
            }
            bool placed = false;
            while (b < dBoxes) {
                if (s >= dSlots) { s = 0; ++b; continue; }
                if (!storageSlotLocked(to, b, s)) {
                    auto& dst = storageSlot(to, b, s);
                    // Treat a non-null species-0 "ghost" slot (an S/V encrypted-blank) as empty, same
                    // as the single-move + returnHeldToOrigin do — otherwise a save-side box that's
                    // full of ghosts has "没有空槽位" and the moved mons get dropped.
                    if (!dst || dst->speciesID() == 0) {
                        convertForPane(srcPtr, to);      // verified convertible above (no-op for the bank)
                        dst = std::move(srcPtr); ++s; placed = true; ++moved; break;
                    }
                }
                ++s;
            }
            if (!placed) { leftover.push_back(src); ++nofit; }
        }
        multiSel = leftover;
        if (moved > 0) hasUnsavedChanges = true;

        // Report what stayed behind: conversion blockers first (actionable — deselect them), then a
        // plain "didn't fit" if the destination simply filled up.
        if (blocked > 0) {
            storageStatus = "其余已移动；有 " + std::to_string(blocked) + " 只无法存入此游戏（"
                          + firstName + (blocked > 1 ? " +" + std::to_string(blocked - 1) : "") + "）";
            storageStatusFrames = 180;
        } else if (nofit > 0) {
            storageStatus = std::to_string(nofit) + " 只宝可梦未能放入：目标盒子已满";
            storageStatusFrames = 180;
        }
    }

    // Green move: drop the whole multi-selection into the destination pane, filling empty slots
    // from (destBox, destSlot) forward. Sources are extracted first (in visual order) so a move
    // within the same pane can't collide with its own sources.
    void TrainerViewScreen::moveSelectionTo(int destPane, int destBox, int destSlot) {
        if (multiSel.empty() || !bank) return;
        // Keep the group's visual order at the destination, and (for a same-pane move) extract every
        // source before writing any slot so a source can't be clobbered by an earlier placement.
        std::vector<SlotRef> order = multiSel;
        std::sort(order.begin(), order.end(), [](const SlotRef& a, const SlotRef& b) {
            return a.box != b.box ? a.box < b.box : a.slot < b.slot;
        });

        // Cross-game into a save (pane 0): convert each foreign mon into the open game's format. Grab
        // the ones that HAVE a route now and leave the rest selected in place, rather than denying the
        // whole drop for one blocker (D5). Bank deposits (destPane 1) never block -- native bytes.
        std::vector<std::unique_ptr<Pokemon::Pokemon>> grabbed;
        std::vector<SlotRef> leftover;
        int blocked = 0;
        std::string firstName;
        const char* firstWhy = nullptr;
        for (const auto& r : order) {
            auto& src = storageSlot(r.pane, r.box, r.slot);
            if (!src) continue;
            if (destPane == 0 && src->getGameGroup() != trainer.getGameGroup()) {
                Conversion::Result res;
                if (!Conversion::canConvert(*src, trainer.getGameGroup(), res)) {
                    ++blocked;
                    if (!firstWhy) {
                        firstName = Names::getDisplayName(src->speciesID(), src->form(),
                                                          Trainer::getSpeciesName(src->speciesID()));
                        firstWhy  = Conversion::resultMessage(res);
                    }
                    leftover.push_back(r);
                    continue;
                }
            }
            grabbed.push_back(std::move(src));
        }
        multiSel = leftover;   // blocked mons stay selected and untouched in their slots

        if (grabbed.empty()) {
            // Nothing had a route -- name the first blocker so the user knows what to do.
            if (blocked > 0) {
                storageStatus = firstName + ": " + firstWhy
                              + (blocked > 1 ? "  (+" + std::to_string(blocked - 1) + " 个未显示)" : "")
                              + " - 无法存入此游戏";
                storageStatusFrames = 180;
            }
            return;
        }

        // Verified convertible above; convert each foreign grabbed mon into the save's format.
        if (destPane == 0)
            for (auto& g : grabbed) convertForPane(g, destPane);

        const int dSlots = (destPane == 0) ? static_cast<int>(trainer.getSlotsPerBox()) : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX);
        const int dBoxes = (destPane == 0) ? static_cast<int>(trainer.getBoxCount()) : static_cast<int>(Trainer::Bank::BANK_BOX_COUNT);
        int b = destBox, s = destSlot;
        size_t idx = 0;
        while (idx < grabbed.size() && b < dBoxes) {
            if (s >= dSlots) { s = 0; ++b; continue; }
            if (!storageSlotLocked(destPane, b, s)) {
                auto& dst = storageSlot(destPane, b, s);
                if (!dst || dst->speciesID() == 0) { dst = std::move(grabbed[idx]); ++idx; }  // species-0 = empty (S/V ghost)
            }
            ++s;
        }
        // Any leftover (destination past the start was full): drop into the first empty slot anywhere.
        for (; idx < grabbed.size(); ++idx) {
            bool placed = false;
            for (int bb = 0; bb < dBoxes && !placed; ++bb)
                for (int ss = 0; ss < dSlots && !placed; ++ss)
                    if (!storageSlotLocked(destPane, bb, ss)) {
                        auto& d = storageSlot(destPane, bb, ss);
                        if (!d || d->speciesID() == 0) { d = std::move(grabbed[idx]); placed = true; }  // species-0 = empty
                    }
        }
        hasUnsavedChanges = true;

        // Some were left behind — say so, or a partial move reads as a full one and the user doesn't
        // notice the blocked mons still sitting selected.
        if (blocked > 0) {
            storageStatus = "其余已移动；有 " + std::to_string(blocked) + " 只无法存入此游戏（"
                          + firstName + (blocked > 1 ? " +" + std::to_string(blocked - 1) : "") + "）";
            storageStatusFrames = 180;
        }
    }

    // Sort one box: order its Pokemon by National Dex no., then form, then level (highest first),
    // and pack them to the front. Empty slots fall to the end.
    //
    // LOCKED SLOTS ARE PINNED and never participate. A save-side slot is locked when a party member
    // points at it (getPartyPosition > 0) -- Let's Go stores the party as INDICES into box storage, so
    // relocating such a slot would silently repoint a party member at a different Pokemon. The
    // occupant stays exactly where it is and the sort flows around it.
    void TrainerViewScreen::sortStorageBox(int pane, int box) {
        const int slots = (pane == 0) ? static_cast<int>(trainer.getSlotsPerBox())
                                      : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX);
        std::vector<int> freeIdx;                                   // slots the sort may write to
        std::vector<std::unique_ptr<Pokemon::Pokemon>> mons;
        for (int s = 0; s < slots; ++s) {
            if (storageSlotLocked(pane, box, s)) continue;          // party-linked: leave untouched
            freeIdx.push_back(s);
            auto& slot = storageSlot(pane, box, s);
            if (slot && slot->speciesID() != 0)                     // species-0 = empty (S/V ghost)
                mons.push_back(std::move(slot));
            slot.reset();
        }
        if (mons.empty()) {
            storageStatus = "此盒子中没有可排序的宝可梦";
            storageStatusFrames = 120;
            return;
        }
        std::sort(mons.begin(), mons.end(),
            [](const std::unique_ptr<Pokemon::Pokemon>& a, const std::unique_ptr<Pokemon::Pokemon>& b) {
                if (a->speciesID() != b->speciesID()) return a->speciesID() < b->speciesID();
                if (a->form() != b->form())           return a->form() < b->form();
                return a->level() > b->level();
            });
        for (size_t i = 0; i < mons.size() && i < freeIdx.size(); ++i)
            storageSlot(pane, box, freeIdx[i]) = std::move(mons[i]);

        hasUnsavedChanges = true;
        storageStatus = "盒子已按图鉴编号排序（" + std::to_string(mons.size()) + ")";
        storageStatusFrames = 120;
    }

    void TrainerViewScreen::handleStorageInput(u64 kDown) {
        if (!bank) return;

        // Accessors that always target the currently-focused pane.
        auto curBox  = [&]() -> int& { return storageFocusPane == 0 ? stSaveBox : stBankBox; };
        auto curSlot = [&]() -> int& { return storageFocusPane == 0 ? stSaveSlot : stBankSlot; };
        auto colsOf  = [&](int pane) { return pane == 0 ? ((static_cast<int>(trainer.getSlotsPerBox()) == 25) ? 5 : 6) : 6; };
        auto slotsOf = [&](int pane) { return pane == 0 ? static_cast<int>(trainer.getSlotsPerBox())
                                                        : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX); };
        auto boxCntOf = [&](int pane) { return pane == 0 ? static_cast<int>(trainer.getBoxCount())
                                                         : static_cast<int>(Trainer::Bank::BANK_BOX_COUNT); };
        // The focused pane's box name is renamable when it's the bank (always) or a save that stores
        // box names (LGPE does not). Gates whether Up/Down can land on the box-name header.
        auto headerRenamable = [&]() { return storageFocusPane == 1 || trainer.supportsBoxNames(); };
        auto slotPtr = [&](int pane, int box, int slot) -> std::unique_ptr<Pokemon::Pokemon>& {
            return pane == 0 ? trainer.boxes[box][slot] : bank->boxes[box][slot];
        };
        // A save-pane slot that is a party member (LGPE) is locked: removing it from storage would
        // orphan its party pointer. No-op for SWSH/LZA (party is a separate structure -> pos 0).
        auto isLocked = [&](int pane, int box, int slot) {
            return pane == 0 && trainer.getPartyPosition(box, slot) > 0;
        };
        auto occupied = [&](int pane, int box, int slot) {
            const auto& p = slotPtr(pane, box, slot);
            return p && p->speciesID() != 0;
        };

        // Y: cycle mode Menu -> Move -> Multi (only with empty hands). Leaving Multi clears the selection.
        const bool holding = (heldPokemon != nullptr);
        if ((kDown & HidNpadButton_Y) && !holding) {
            cursorMode = cursorMode == CursorMode::Menu ? CursorMode::Move
                       : cursorMode == CursorMode::Move ? CursorMode::Multi : CursorMode::Menu;
            if (cursorMode != CursorMode::Multi) multiSel.clear();
        }

        // L/R: change the focused pane's box (wrap); keep the cursor slot in range.
        if (kDown & HidNpadButton_L) curBox() = (curBox() - 1 + boxCntOf(storageFocusPane)) % boxCntOf(storageFocusPane);
        if (kDown & HidNpadButton_R) curBox() = (curBox() + 1) % boxCntOf(storageFocusPane);
        if (curSlot() >= slotsOf(storageFocusPane)) curSlot() = slotsOf(storageFocusPane) - 1;

        // D-pad: move within the focused pane; Left/Right cross panes at the horizontal edges.
        const int cols = colsOf(storageFocusPane);
        const int slots = slotsOf(storageFocusPane);
        constexpr int rows = 5;
        // curSlot() == -1 is the "box-name header focused" state (rename via A or a tap). Up from the
        // top row lands on it and Down leaves it -- but only where the header is renamable and the
        // hands are empty, so LGPE (no box names) and mid-carry keep the plain top<->bottom wrap.
        if (kDown & HidNpadButton_Up) {
            if (curSlot() == -1) {
                curSlot() = (rows - 1) * cols;                        // header -> bottom row (wrap)
                if (curSlot() >= slots) curSlot() = slots - 1;
            } else {
                int r = curSlot() / cols, c = curSlot() % cols;
                if (r == 0 && headerRenamable() && !holding) {
                    curSlot() = -1;                                  // top row -> header
                } else {
                    r = (r - 1 + rows) % rows;
                    curSlot() = r * cols + c;
                    if (curSlot() >= slots) curSlot() = slots - 1;
                }
            }
        }
        if (kDown & HidNpadButton_Down) {
            if (curSlot() == -1) {
                curSlot() = 0;                                        // header -> top-left
            } else {
                int r = curSlot() / cols, c = curSlot() % cols;
                if (r == rows - 1 && headerRenamable() && !holding) {
                    curSlot() = -1;                                  // bottom row -> header (wrap)
                } else {
                    r = (r + 1) % rows;
                    curSlot() = r * cols + c;
                    if (curSlot() >= slots) curSlot() = slots - 1;
                }
            }
        }
        if (kDown & HidNpadButton_Left) {
            if (curSlot() == -1) {                                    // on the header: ◀ cycles the box
                curBox() = (curBox() - 1 + boxCntOf(storageFocusPane)) % boxCntOf(storageFocusPane);
            } else {
                int c = curSlot() % cols;
                if (c > 0) {
                    curSlot() -= 1;
                } else if (storageFocusPane == 1) {
                    const int r = curSlot() / cols;
                    storageFocusPane = 0;
                    const int scols = colsOf(0);
                    stSaveSlot = r * scols + (scols - 1);
                    if (stSaveSlot >= slotsOf(0)) stSaveSlot = slotsOf(0) - 1;
                }
            }
        }
        if (kDown & HidNpadButton_Right) {
            if (curSlot() == -1) {                                    // on the header: ▶ cycles the box
                curBox() = (curBox() + 1) % boxCntOf(storageFocusPane);
            } else {
                int c = curSlot() % cols;
                if (c < cols - 1 && curSlot() + 1 < slots) {
                    curSlot() += 1;
                } else if (storageFocusPane == 0) {
                    const int r = curSlot() / cols;
                    storageFocusPane = 1;
                    stBankSlot = r * colsOf(1);  // col 0 of the bank pane
                }
            }
        }

        const int pane = storageFocusPane, box = curBox(), slot = curSlot();

        // Minus: in Multi mode with a selection, open the group menu.
        if ((kDown & HidNpadButton_Minus) && !holding &&
            cursorMode == CursorMode::Multi && !multiSel.empty()) {
            groupMenuActive = true;
            groupMenuIndex = 0;
            return;
        }

        // X: sort the focused box. Free here -- the global X-to-save handler deliberately skips
        // the storage view, since the bank saves via its own B -> Save/Discard prompt. Suppressed while
        // carrying a Pokemon, so a sort can never run with one held out of the grid and lose it.
        if ((kDown & HidNpadButton_X) && !holding) {
            sortStorageBox(storageFocusPane, storageFocusPane == 0 ? stSaveBox : stBankBox);
            return;
        }

        // A: act. Holding overrides the mode (place/swap the carried Pokemon; menu suppressed).
        if (kDown & HidNpadButton_A) {
            // Box-name header focused: A renames this pane's box (save box, or bank box). Guard first
            // so the placement/menu code below never indexes storage with slot == -1.
            if (slot == -1) {
                if (!holding) { if (pane == 0) renameBox(box); else renameBankBox(box); }
                return;
            }
            if (holding) {
                // Confirm first for a lossy conversion: a Gen 3 downgrade rebuilds the PID (always warned),
                // or -- settings-gated -- a Let's Go cross-move that resets AVs/EVs.
                const bool g3warn = !isLocked(pane, box, slot) && gen3DowngradeInvolved(pane, heldPokemon.get());
                const bool lgwarn = g_lgpeMoveWarn && !isLocked(pane, box, slot) && lgpeConversionInvolved(pane, heldPokemon.get());
                if (g3warn || lgwarn) {
                    lgpePending = LgpePending::PlaceHeld;
                    lgpePendPane = pane; lgpePendBox = box; lgpePendSlot = slot;
                    moveConfirmGen3 = g3warn; lgpeMoveConfirmActive = true; lgpeMoveConfirmIndex = 1;
                    return;
                }
                if (!isLocked(pane, box, slot) && prepareHeldForPane(pane)) {
                    auto& here = slotPtr(pane, box, slot);
                    if (!occupied(pane, box, slot)) {
                        here = std::move(heldPokemon);
                    } else {
                        std::swap(here, heldPokemon);
                        heldPane = pane; heldFromBox = box; heldFromSlot = slot;
                    }
                    hasUnsavedChanges = true;
                }
                return;
            }
            switch (cursorMode) {
                case CursorMode::Menu:
                    if (occupied(pane, box, slot)) {
                        // Party-linked (locked) slots still open the menu -- Edit and Clone are valid
                        // there; only Move and Release are disabled (see the action handler + the
                        // greyed rows). Start on Edit when Move is unavailable so the cursor doesn't
                        // land on a greyed row.
                        storageMenuActive = true;
                        storageMenuIndex = isLocked(pane, box, slot) ? 1 : 0;
                        menuPane = pane; menuBox = box; menuSlot = slot;
                    } else if (pane == 0 && !occupied(pane, box, slot) && !isLocked(pane, box, slot)) {
                        // Empty save-pane slot: create a new Pokemon here (pick species, then edit).
                        creator.active = true; creator.pane = pane; creator.box = box; creator.slot = slot;
                        pickerKind = Dialogs::PickerKind::Species;
                        buildCreatorSpeciesOrder();  // only species obtainable in this game (unless illegal-values on)
                        pickerCount = static_cast<int>(pickerOrder.size());
                        pickerActive = true;
                    }
                    break;
                case CursorMode::Move:
                    if (occupied(pane, box, slot) && !isLocked(pane, box, slot)) {
                        heldPokemon = std::move(slotPtr(pane, box, slot));
                        heldPane = pane; heldFromBox = box; heldFromSlot = slot;
                    }
                    break;
                case CursorMode::Multi:
                    if (occupied(pane, box, slot) && !isLocked(pane, box, slot)) {
                        // Occupied slot: toggle it in the selection (single-pane; crossing resets it).
                        if (!multiSel.empty() && multiSel.front().pane != pane) multiSel.clear();
                        bool found = false;
                        for (size_t i = 0; i < multiSel.size(); ++i)
                            if (multiSel[i].pane == pane && multiSel[i].box == box && multiSel[i].slot == slot) {
                                multiSel.erase(multiSel.begin() + i); found = true; break;
                            }
                        if (!found) multiSel.push_back({pane, box, slot});
                    } else if (!occupied(pane, box, slot) && !isLocked(pane, box, slot) && !multiSel.empty()) {
                        // Empty slot with a selection: move the whole group here (cross-pane = deposit/withdraw).
                        const bool g3warn = selectionInvolvesGen3Downgrade(pane);
                        const bool lgwarn = g_lgpeMoveWarn && selectionInvolvesLgpe(pane);
                        if (g3warn || lgwarn) {   // confirm a lossy conversion (Gen 3 PID rebuild / LGPE AV-EV reset)
                            lgpePending = LgpePending::GroupMoveTo;
                            lgpePendPane = pane; lgpePendBox = box; lgpePendSlot = slot;
                            moveConfirmGen3 = g3warn; lgpeMoveConfirmActive = true; lgpeMoveConfirmIndex = 1;
                        } else {
                            moveSelectionTo(pane, box, slot);
                        }
                    }
                    break;
            }
        }
    }

    void TrainerViewScreen::update(const PadState& pad, const TouchInput& touch) {
        u64 kDown = padGetButtonsDown(&pad) | navTouchButton(touch);   // nav-bar badges are tappable

        // Let's Go stores its boxes as a GAPLESS list, so anything that vacated a slot last frame
        // left a hole the game can't represent. Re-pack it here rather than at each of the
        // several sites that can vacate a slot: this function has many early returns -- the release
        // path returns before ever reaching the bottom -- so an end-of-update hook would silently
        // skip them, and per-site hooks are exactly the coverage gap this codebase keeps hitting.
        // A no-op on every positional game. Excluded mid-operation so the board can't shift under a
        // Pokemon the user is currently carrying, swapping or creating.
        if (!heldPokemon && !swapActive && !creator.active && trainer.compactStorage()) {
            multiSel.clear();   // entries are (pane, box, slot) refs; a re-pack invalidates them
        }

        // Keep the game's persisted "当前盒子" in step with whichever box view is open, so it
        // survives leaving Storage before the game save runs. In Storage X sorts, so the save happens
        // after backing out -- by which point the mode has changed; capturing the box here (not at
        // save time) is what makes the Storage box persist, matching the Boxes view. The save-pane box
        // (stSaveBox) and the Boxes-view box (selectedBoxIndex) are two cursors on the same current
        // box; gated on detailViewActive so the HOME menu (no box view) can't overwrite it.
        if (detailViewActive) {
            const int bc = static_cast<int>(trainer.getBoxCount());
            if (selectedMode == ViewMode::Storage && stSaveBox >= 0 && stSaveBox < bc)
                trainer.setCurrentBox(static_cast<uint8_t>(stSaveBox));
            else if (selectedMode == ViewMode::Boxes && selectedBoxIndex >= 0 && selectedBoxIndex < bc)
                trainer.setCurrentBox(static_cast<uint8_t>(selectedBoxIndex));
        }

        // Expire the transient status line. This lives HERE, not in handleStorageInput, because that
        // function only runs in the storage view and returns early without a bank -- so a message
        // posted from anywhere else (a save result, a box rename) would have stayed on screen
        // forever. update() runs every frame in every view, which is what a timer needs.
        if (storageStatusFrames > 0) --storageStatusFrames;

        // A box-name pill was tapped last frame: the header highlight has now been drawn once, so it
        // is safe to open the (blocking) rename keyboard. Doing this inline with the tap suspended the
        // app before the highlight ever rendered, so the selection only lit up AFTER the dialog closed
        // -- the deferral is what makes the tap feel immediate. One frame's delay is imperceptible.
        if (pendingHeaderRename) {
            pendingHeaderRename = false;
            if (selectedMode == ViewMode::Storage) {
                if (storageFocusPane == 0) renameBox(stSaveBox);
                else                       renameBankBox(stBankBox);
            } else {
                renameBox(selectedBoxIndex);
            }
            return;
        }

        // Handle + button (exits application)
        if (kDown & HidNpadButton_Plus) {
            // The bank question is already on screen -- answer that first rather than stacking a
            // second exit on top of it.
            if (storageExitConfirmActive) return;
            // A carried Pokemon goes home before anything is weighed up.
            returnHeldToOrigin();
            // The bank is its own save file and gets its own decision. Closing the app is NOT that
            // decision, so it must not write the bank on the way out: doing that committed every
            // transfer even when the user went on to DECLINE the game save, and the two files then
            // disagreed. A deposit became a permanent CLONE (the bank has it, the game save still
            // has it), a withdrawal a permanent LOSS (the bank no longer has it, the game save was
            // never written). Ask instead, and let Discard rewind both sides and write nothing.
            if (bank && bank->hasChanged()) {
                storageExitConfirmActive = true;
                storageExitConfirmIndex = 0;
                exitAfterBankChoice = true;   // resume this exit once they've answered
                return;
            }
            beginAppExit();
            return;
        }

        // Reusable value picker (nature / gender / move) — owns all input while open.
        if (pickerActive) {
            Pokemon::Pokemon* pkPick = detailsTargetPokemon();
            const int count = (pickerCount > 0) ? pickerCount : 1;
            const int page = 12;

            // Touch: tap an option row selects it immediately (button id = option index).
            int ptb = touchedButtonId(touch);
            if (ptb >= 0 && ptb < count) { pickerSel = ptb; kDown |= HidNpadButton_A; }

            if (kDown & HidNpadButton_Up)                        pickerSel = (pickerSel - 1 + count) % count;
            if (kDown & HidNpadButton_Down)                      pickerSel = (pickerSel + 1) % count;
            if (kDown & (HidNpadButton_L | HidNpadButton_Left))  pickerSel = std::max(0, pickerSel - page);
            if (kDown & (HidNpadButton_R | HidNpadButton_Right)) pickerSel = std::min(count - 1, pickerSel + page);
            if (pickerSel < 0) pickerSel = 0;
            if (pickerSel >= count) pickerSel = count - 1;

            if (kDown & HidNpadButton_B) {
                pickerActive = false; creator.active = false;
                // A change-type picker was launched from the Edit Item dialog -> cancelling it
                // returns THERE, not to the item list (the dialog is still "打开" underneath).
                if (itemPickerReplace) { itemPickerReplace = false; itemEditDialogActive = true; }
                return;
            }

            // Creator: a Species pick in create mode builds a new mon into the target empty slot,
            // then hands off to the details editor (there is no live target Pokemon yet, so this
            // intercepts before the normal pkPick apply-switch below).
            if ((kDown & HidNpadButton_A) && creator.active && pickerKind == Dialogs::PickerKind::Species) {
                const int sp = (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size()))
                                   ? pickerOrder[pickerSel] : pickerSel;
                if (sp > 0) {
                    storageSlot(creator.pane, creator.box, creator.slot) =
                        buildDefaultMon(trainer, static_cast<uint16_t>(sp),
                                        creatorOriginVersion(trainer, titleId));
                    hasUnsavedChanges = true;
                    pickerActive = false; creator.active = false;
                    openStorageEditor(creator.pane, creator.box, creator.slot);
                    creator.editing = true;  // modal now edits a fresh mon -> Keep/Discard prompt on exit
                } else {
                    pickerActive = false; creator.active = false;  // "无" cancels
                }
                return;
            }

            // Change item type: a pouch pick opened from the Edit Item dialog REPLACES the selected
            // item's type rather than adding a new one. Must come before the add handler (same kind).
            if ((kDown & HidNpadButton_A) && itemPickerReplace
             && (pickerKind == Dialogs::PickerKind::PouchItem
              || pickerKind == Dialogs::PickerKind::PouchItemG3)) {
                itemPickerReplace = false;
                if (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size())
                    && selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    const uint16_t newId = static_cast<uint16_t>(pickerOrder[pickerSel]);
                    auto& pouch = trainer.items[selectedCategory];
                    std::vector<int> visible = visibleItemIndices();
                    if (selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(visible.size())) {
                        const int rawIdx = visible[selectedItemIndex];
                        const uint16_t keepCount = pouch[rawIdx].count;
                        // The dialog we return to must show the NEW item's amount (its count carried
                        // over unchanged), so re-sync both the live value and the change-baseline.
                        itemEditDialogValue = keepCount;
                        itemEditDialogOriginalValue = keepCount;
                        if (trainer.itemsAreIdIndexed()) {
                            // Zero the old id (KEEP the entry so its slot is written to 0), then set the
                            // new id -- reassigning itemId in place would ghost the old id's count.
                            pouch[rawIdx].count = 0;
                            const auto e = std::find_if(pouch.begin(), pouch.end(),
                                [newId](const Trainer::InventoryItem& v) { return v.itemId == newId; });
                            if (e != pouch.end()) { e->count = keepCount; e->isNew = true; }
                            else pouch.push_back(Trainer::InventoryItem{ newId, keepCount, true, false });
                        } else {
                            pouch[rawIdx].itemId = newId;   // slot-based: reassign in place, region rewritten
                            pouch[rawIdx].isNew = true;     // the swapped-in item shows as new
                        }
                        hasUnsavedChanges = true;
                        // Land the cursor on the changed item (it may have re-sorted onto another page).
                        const std::vector<int> vis = visibleItemIndices();
                        int perPage = (CONTENT_PANEL_HEIGHT - 106) / 52;
                        if (perPage < 1) perPage = 1;
                        for (int i = 0; i < static_cast<int>(vis.size()); ++i) {
                            if (pouch[vis[i]].itemId == newId) { selectedItemIndex = i; currentPage = i / perPage; break; }
                        }
                    }
                }
                pickerActive = false;
                itemEditDialogActive = true;   // return to the Edit Item dialog, now showing the new type
                return;
            }

            // Item creation: a pouch pick edits the trainer's BAG, not a Pokemon, so it has to
            // intercept before the apply-switch below -- pkPick is null in the items view.
            if ((kDown & HidNpadButton_A)
             && (pickerKind == Dialogs::PickerKind::PouchItem
              || pickerKind == Dialogs::PickerKind::PouchItemG3)) {
                if (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size())
                    && selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    const uint16_t id = static_cast<uint16_t>(pickerOrder[pickerSel]);
                    auto& pouch = trainer.items[selectedCategory];
                    const auto e = std::find_if(pouch.begin(), pouch.end(),
                        [id](const Trainer::InventoryItem& x) { return x.itemId == id; });
                    if (e != pouch.end()) {
                        // Present but empty -> re-activate it AND flag it new (a freshly-added
                        // item always shows the bag's "新建" marker in the games that have one).
                        if (e->count == 0) { e->count = 1; e->isNew = true; hasUnsavedChanges = true; }
                    } else if (static_cast<int>(pouch.size()) < currentPouchCapacity()) {
                        pouch.push_back(Trainer::InventoryItem{ id, 1, true, false });
                        hasUnsavedChanges = true;
                    }
                    // Land the cursor on the row that was just added so A edits its amount next --
                    // and FOLLOW IT TO ITS PAGE. The list is paged, and a new item usually sorts
                    // onto a later page, so selecting it without moving the page left the user
                    // staring at an unchanged screen having to go find it themselves.
                    const std::vector<int> vis = visibleItemIndices();
                    int perPage = (CONTENT_PANEL_HEIGHT - 106) / 52;   // mirrors ItemsPanel's row fit
                    if (perPage < 1) perPage = 1;
                    for (int i = 0; i < static_cast<int>(vis.size()); ++i) {
                        if (pouch[vis[i]].itemId == id) {
                            selectedItemIndex = i;
                            currentPage = i / perPage;
                            break;
                        }
                    }
                }
                pickerActive = false;
                return;
            }

            if ((kDown & HidNpadButton_A) && pkPick) {
                switch (pickerKind) {
                    case Dialogs::PickerKind::Nature:
                        pkPick->setNature(static_cast<uint8_t>(pickerSel));
                        pkPick->setStatNature(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Gender:
                        pkPick->setGender(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Move: {
                        const int mv = (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size()))
                                           ? pickerOrder[pickerSel] : pickerSel;
                        pkPick->setMove(pickerSlot, static_cast<uint16_t>(mv));
                        // Setting a move never sets its PP (needs a base-PP table); do it here so a
                        // picked move isn't stuck at 0 PP. PP-ups reset to 0 -> base PP (#F1F2).
                        pkPick->setMovePP(pickerSlot, Names::getMoveBasePP(static_cast<uint16_t>(mv)));
                        pkPick->setMovePPUps(pickerSlot, 0);
                        break;
                    }
                    case Dialogs::PickerKind::Item:
                    case Dialogs::PickerKind::ItemG3:   // same write; only the id space differs
                        pkPick->setHeldItem(static_cast<uint16_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Level:
                        pkPick->setLevel(static_cast<uint8_t>(pickerSel + 1));  // options 0-99 -> level 1-100
                        break;
                    case Dialogs::PickerKind::Friendship:
                        pkPick->setFriendship(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Ball:
                        if (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size()))
                            pkPick->setBall(static_cast<uint8_t>(pickerOrder[pickerSel]));
                        break;
                    case Dialogs::PickerKind::Ability: {
                        const int v = (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size()))
                                          ? pickerOrder[pickerSel] : pickerSel;
                        pkPick->setAbility(static_cast<uint16_t>(v));
                        // Align the ability slot when the pick is one of the species' legal abilities.
                        const Pokemon::PersonalInfo& pai = Pokemon::getPersonalInfo(pkPick->speciesID(), pkPick->form());
                        if (v == pai.ability1)           pkPick->setAbilityNumber(1);
                        else if (v == pai.ability2)      pkPick->setAbilityNumber(2);
                        else if (v == pai.abilityHidden) pkPick->setAbilityNumber(4);
                        break;
                    }
                    case Dialogs::PickerKind::Language:
                        pkPick->setLanguage(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Origin:
                        pkPick->setOriginGame(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::MetLevel:
                        pkPick->setMetLevel(static_cast<uint8_t>(pickerSel + 1));  // options 0-99 -> met level 1-100
                        break;
                    case Dialogs::PickerKind::MetLocation:
                        if (!pickerOrder.empty() && pickerSel < static_cast<int>(pickerOrder.size())) {
                            uint16_t loc = static_cast<uint16_t>(pickerOrder[pickerSel]);
                            if (pickerMetIsEgg) pkPick->setEggLocation(loc);   // egg-met vs met location
                            else                pkPick->setMetLocation(loc);
                        }
                        break;
                    case Dialogs::PickerKind::Form:
                        pkPick->setForm(static_cast<uint8_t>(pickerSel));  // recalculates stats + checksum internally
                        break;
                    case Dialogs::PickerKind::StatNature:
                        pkPick->setStatNature(static_cast<uint8_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::Species:
                        pkPick->setSpecies(static_cast<uint16_t>(pickerSel));
                        break;
                    case Dialogs::PickerKind::PouchItem:
                    case Dialogs::PickerKind::PouchItemG3:
                        break;   // handled above -- these edit the bag, not a Pokemon
                }
                hasUnsavedChanges = true;
                mirrorEditedPartyMember();
                pickerActive = false;
            }
            return;
        }

        // X saves the game -- but ONLY from the HOME main menu (where you pick Pokemon / Party /
        // Storage / Items / ...). detailViewActive means "entered a view", and inside any entered view
        // X is a view-local action (Items: remove an item; Storage: sort box; the details page: save the
        // mon), so the global game-save must never fire there. Back out to the menu, then press X.
        if (kDown & HidNpadButton_X) {
            const bool onHomeMenu = !detailViewActive;
            if (onHomeMenu && !saveConfirmActive && !itemEditDialogActive && !statEdit.dialogActive &&
                !releaseConfirmActive && !storageMenuActive && !groupMenuActive &&
                !storageExitConfirmActive && !details.active) {
                exitingWithUnsavedChanges = false;  // Regular save, not exiting
                exitingViaPlus = false;
                saveDestIndex = defaultSaveDest();   // cart session -> game save; backup -> that backup
                saveConfirmActive = true;
                return;
            }
        }

        // Injecting into the real game save — the last gate, and the only thing PKSE does that
        // overwrites data the user cannot recover from inside the app. Handled BEFORE the save
        // dialog because saveConfirmActive is still true underneath it.
        if (saveInjectConfirmActive) {
            const int tb = touchedButtonId(touch);
            if (tb == 1)      kDown |= HidNpadButton_A;
            else if (tb == 0) kDown |= HidNpadButton_B;
            if (kDown & HidNpadButton_A) {
                saveInjectConfirmActive = false;
                performSave(backupDir, true);
                return;
            }
            if (kDown & HidNpadButton_B) { saveInjectConfirmActive = false; return; }  // back to the picker
            return;
        }

        // Handle save confirmation dialog
        if (saveConfirmActive) {
            if (exitingWithUnsavedChanges) {
                // Exiting with unsaved changes - different button logic
                // A: Discard changes and exit
                // B: Cancel exit (stay on screen)
                // Tappable buttons: id 0 = Cancel (B), id 1 = Discard & Exit (A).
                const int td = touchedButtonId(touch);
                if (td == 0) kDown |= HidNpadButton_B;
                else if (td == 1) kDown |= HidNpadButton_A;
                if (kDown & HidNpadButton_A) {
                    // User chose to discard changes and exit
                    saveConfirmActive = false;
                    if (exitingViaPlus) {
                        // Exiting via + button - exit the app
                        exitRequested = true;
                    }
                    // Always set goBack for consistency (B button case)
                    goBack = true;
                    exitingWithUnsavedChanges = false;
                    exitingViaPlus = false;
                    return;
                }
                if (kDown & HidNpadButton_B) {
                    // User cancelled exit - stay on screen
                    saveConfirmActive = false;
                    exitingWithUnsavedChanges = false;
                    exitingViaPlus = false;
                    return;
                }
            } else {
                // Regular save dialog (triggered by X button)
                // A: Save changes
                // B: Cancel
                // Destination picker: Up/Down choose, A writes there.
                const int nDest = saveDestCount();
                if (saveDestIndex >= nDest) saveDestIndex = DestThisBackup;   // lock may have gone off
                if (kDown & HidNpadButton_Up)   saveDestIndex = (saveDestIndex - 1 + nDest) % nDest;
                if (kDown & HidNpadButton_Down) saveDestIndex = (saveDestIndex + 1) % nDest;
                const int td = touchedButtonId(touch);
                if (td >= 0 && td < nDest) { saveDestIndex = td; kDown |= HidNpadButton_A; }

                if (kDown & HidNpadButton_A) {
                    // A carried Pokemon lives outside both containers — return it before serializing
                    // so it isn't dropped from the save/bank.
                    returnHeldToOrigin();

                    // (The bank has its OWN persistence — it saves on storage-view exit / app exit,
                    // separate from this game-save, since it's a separate entity from the save file.)
                    if (saveDestIndex == DestGameSave) {
                        // Writing your OWN save back is the ordinary thing a save editor does -- the
                        // data being overwritten is the data we just read -- so it goes straight
                        // through. Only a backup-sourced session needs the extra gate, because that
                        // is the one that rolls the game backwards.
                        if (loadedFromCart) { performSave(backupDir, true); return; }
                        saveInjectConfirmActive = true;
                        return;
                    }

                    std::string destDir = backupDir;
                    if (saveDestIndex == DestNewBackup) {
                        const Utils::KeyboardResult res =
                            Utils::promptText("新备份", "备份名称", titleName, 40);
                        if (!res.accepted) return;      // cancelled: leave the dialog up, nothing written
                        destDir = createNamedBackupDir(res.text);
                        if (destDir.empty()) {
                            postStatus("无法创建该备份。请仅使用字母、数字、空格或 -。", 480);
                            return;
                        }
                    }
                    performSave(destDir, false);
                    return;
                }
                if (kDown & HidNpadButton_B) {
                    // User cancelled save - just close the dialog
                    saveConfirmActive = false;
                    return;
                }
            }
            return;  // Don't process other inputs while save confirm is active
        }

        // Handle stat edit dialog
        if (statEdit.dialogActive) {
            // Touch: map on-screen buttons to their equivalent presses (IV/EV rows -> Up/Down mode
            // switch, the step buttons -> ZL/L/Left/Right/R/ZR, Save -> A, Cancel -> B), then reuse
            // the button logic below.
            switch (touchedButtonId(touch)) {
                case 10: kDown |= HidNpadButton_Up;    break;
                case 11: kDown |= HidNpadButton_Down;  break;
                case 34: kDown |= HidNpadButton_ZL;    break;
                case 30: kDown |= HidNpadButton_L;     break;
                case 31: kDown |= HidNpadButton_Left;  break;
                case 32: kDown |= HidNpadButton_Right; break;
                case 33: kDown |= HidNpadButton_R;     break;
                case 35: kDown |= HidNpadButton_ZR;    break;
                case 1:  kDown |= HidNpadButton_A;     break;
                case 0:  kDown |= HidNpadButton_B;     break;
                default: break;
            }

            // Resolve the Pokemon being edited (party / box / bank).
            Pokemon::Pokemon* pokemon = detailsTargetPokemon();

            if (pokemon) {
                    // Let's Go uses Awakening Values (AVs), not EVs — EVs are inert there. So the
                    // second editable stat is AV for LGPE Pokemon, EV for everything else.
                    const bool usesAV = pokemon->hasAwakeningValues();
                    const Dialogs::StatEditMode secondMode = usesAV ? Dialogs::StatEditMode::AV
                                                                    : Dialogs::StatEditMode::EV;

                    // Up/Down to switch between IV and the second stat (EV / AV)
                    if (kDown & HidNpadButton_Up) {
                        // Save current value before switching
                        if (statEdit.mode == secondMode) {
                            if (usesAV) statEdit.currentAV = statEdit.value;
                            else        statEdit.currentEV = statEdit.value;
                        }
                        statEdit.mode = Dialogs::StatEditMode::IV;
                        statEdit.value = statEdit.currentIV;  // Load IV value
                    }
                    if (kDown & HidNpadButton_Down) {
                        // Save current value before switching
                        if (statEdit.mode == Dialogs::StatEditMode::IV) {
                            statEdit.currentIV = statEdit.value;
                        }
                        statEdit.mode = secondMode;
                        statEdit.value = usesAV ? statEdit.currentAV : statEdit.currentEV;  // Load AV/EV value
                    }

                    // Get max value based on mode (IV 0-31, AV 0-200, EV 0-252)
                    int minValue = 0;
                    // Illegal-values override (Settings) lifts the EV/AV per-stat cap to 255. IV stays 31
                    // (the 5-bit format ceiling — there is no "illegal" IV value to reach).
                    int maxValue = (statEdit.mode == Dialogs::StatEditMode::IV) ? 31
                                 : (statEdit.mode == Dialogs::StatEditMode::AV) ? (g_allowIllegalEdits ? 255 : 200)
                                 : (g_allowIllegalEdits ? 255 : 252);

                    // Adjust value: Left/Right = -/+1, L/R = -/+10, ZL/ZR = -/+100. On a 0-31 IV the
                    // +/-100 steps just clamp, which is harmless.
                    if (kDown & HidNpadButton_Left)  statEdit.value = std::max(minValue, statEdit.value - 1);
                    if (kDown & HidNpadButton_Right) statEdit.value = std::min(maxValue, statEdit.value + 1);
                    if (kDown & HidNpadButton_L)     statEdit.value = std::max(minValue, statEdit.value - 10);
                    if (kDown & HidNpadButton_R)     statEdit.value = std::min(maxValue, statEdit.value + 10);
                    if (kDown & HidNpadButton_ZL)    statEdit.value = std::max(minValue, statEdit.value - 100);
                    if (kDown & HidNpadButton_ZR)    statEdit.value = std::min(maxValue, statEdit.value + 100);

                    // Store back into whichever value the active mode edits.
                    if (statEdit.mode == Dialogs::StatEditMode::IV)      statEdit.currentIV = statEdit.value;
                    else if (statEdit.mode == Dialogs::StatEditMode::AV) statEdit.currentAV = statEdit.value;
                    else                                                statEdit.currentEV = statEdit.value;

                    // For EVs, enforce the 510 legal total — unless the illegal-values override is on
                    // (510 is a game rule, not a byte limit; the illegal ceiling is 6 x 255 = 1530).
                    if (statEdit.mode == Dialogs::StatEditMode::EV && !g_allowIllegalEdits) {
                        int totalEVs = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
                            pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD();
                        int projectedTotal = totalEVs - statEdit.originalEV + statEdit.value;
                        if (projectedTotal > 510) {
                            // Adjust value to not exceed total
                            statEdit.value = std::max(0, 510 - (totalEVs - statEdit.originalEV));
                        }
                    }

                    // Confirm edit
                    if (kDown & HidNpadButton_A) {
                        // Remember that something outside the games' own limits is being committed,
                        // so the save can warn ONCE rather than the editor nagging per field. Only
                        // reachable with "允许非法数值" on; the caps clamp otherwise.
                        const int evTotalAfter = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
                                                 pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD()
                                               - statEdit.originalEV + statEdit.currentEV;
                        if (statEdit.currentEV > 252 || statEdit.currentAV > 200 || evTotalAfter > 510)
                            illegalDataWritten = true;

                        // Map UI stat index to Pokemon data stat index
                        // UI order: HP, ATK, DEF, SPA, SPD, SPE (indices 0-5)
                        // Data order: HP, ATK, DEF, SPE, SPA, SPD (indices 0-5)
                        int statIndexMap[] = {0, 1, 2, 4, 5, 3}; // UI index -> Data index
                        int dataStatIndex = statIndexMap[statEdit.selectedStat];

                        // Apply IV always; apply AV for Let's Go, EV otherwise.
                        bool ivEvModified = false;  // IV/EV changes want a PID refresh; AV doesn't
                        bool anyModified = false;
                        if (statEdit.currentIV != statEdit.originalIV) {
                            pokemon->setIV(dataStatIndex, statEdit.currentIV);
                            hasUnsavedChanges = true;
                            ivEvModified = true; anyModified = true;
                        }
                        if (usesAV) {
                            if (statEdit.currentAV != statEdit.originalAV) {
                                pokemon->setAV(dataStatIndex, static_cast<uint8_t>(statEdit.currentAV));
                                hasUnsavedChanges = true;
                                anyModified = true;
                            }
                        } else if (statEdit.currentEV != statEdit.originalEV) {
                            pokemon->setEV(dataStatIndex, static_cast<uint8_t>(statEdit.currentEV));
                            hasUnsavedChanges = true;
                            ivEvModified = true; anyModified = true;
                        }

                        // Regenerate PID to maintain legality after IV/EV changes (AVs don't affect it).
                        if (ivEvModified) {
                            pokemon->regeneratePID(pokemon->id32());
                        }
                        // If this is an LGPE party member, mirror the edit to its party copy so the
                        // save's party overlay doesn't clobber it.
                        if (anyModified) {
                            mirrorEditedPartyMember();
                        }

                        statEdit.dialogActive = false;
                        return;
                    }

                    // Cancel edit
                    if (kDown & HidNpadButton_B) {
                        statEdit.dialogActive = false;
                        return;
                    }
            }

            return;  // Don't process other inputs while stat edit dialog is active
        }

        // Handle Pokemon details modal (HOME Check-Summary editor page).
        if (details.active) {
            Pokemon::Pokemon* pokemon = detailsTargetPokemon();
            int tb = touchedButtonId(touch);

            // Creator: a freshly-made mon isn't committed until accepted. Closing raises this prompt --
            // Keep (A / right button) accepts it; Discard (B / left button) removes it from the box.
            if (creator.keepConfirmActive) {
                // Three glyph buttons: id 0 = Back (B), id 2 = Discard (Y), id 1 = Keep (A).
                if (tb == 0)      kDown |= HidNpadButton_B;
                else if (tb == 1) kDown |= HidNpadButton_A;
                else if (tb == 2) kDown |= HidNpadButton_Y;
                if (kDown & HidNpadButton_B) { creator.keepConfirmActive = false; return; }  // back to editing the new mon
                const bool discard = (kDown & HidNpadButton_Y);
                if ((kDown & HidNpadButton_A) || discard) {
                    if (discard) {  // remove the just-created mon from the box
                        storageSlot(creator.pane, creator.box, creator.slot).reset();
                        hasUnsavedChanges = true;
                    }
                    creator.keepConfirmActive = false; creator.editing = false;
                    details.active = false; details.source = EditSource::Box;
                    details.selectedField = 0; details.legalityOverlay = false; details.ribbonOverlay = false;
                }
                return;
            }

            // Legality overlay (full issue list) intercepts input while open: B or any tap closes it.
            if (details.legalityOverlay) {
                if ((kDown & HidNpadButton_B) || tb >= 0) details.legalityOverlay = false;
                return;
            }
            // Ribbon overlay (full ribbon/mark list) does the same.
            if (details.ribbonOverlay) {
                if ((kDown & HidNpadButton_B) || tb >= 0) details.ribbonOverlay = false;
                return;
            }
            // Open the ribbon list: Y, or tapping the Ribbons row (id 94).
            if ((kDown & HidNpadButton_Y) || tb == 94) {
                details.ribbonOverlay = true;
                return;
            }
            // Open the legality issue list: R, or tapping the legality summary (id 95) -- but ONLY
            // when the mon actually has issues. A clean mon has nothing to show, so the button is
            // disabled (greyed in the guide) and this does nothing.
            if ((kDown & HidNpadButton_R) || tb == 95) {
                if (pokemon && !Legality::analyze(*pokemon, pokemon->getGameGroup()).ok())
                    details.legalityOverlay = true;
                return;
            }
            // X: for an EXISTING mon, SAVE the edits -- commit the current state as the new baseline
            // (the "未保存的更改" marker clears) and STAY on the page. This is the ONLY commit:
            // closing with B/close rolls back to this baseline, so any later edits are discarded. For a
            // freshly-CREATED mon there is nothing to "保存": every field is an unsaved edit until
            // committed, so X is KEEP -- finalize the mon (already in the box) and close. Discard still
            // lives on the B Keep/Discard prompt.
            if (kDown & HidNpadButton_X) {
                if (creator.editing) { creator.editing = false; closeDetailsModal(); return; }
                snapshotEditTarget();   // baseline := current -> pokemonEditDirty() now false
                return;
            }
            // Randomize IVs: L only. (The on-panel Randomize row was removed; L is the trigger,
            // and the bottom nav bar advertises it.)
            if (kDown & HidNpadButton_L) {
                if (Pokemon::Pokemon* target = detailsTargetPokemon()) {
                    randomizeIVs(target);
                    hasUnsavedChanges = true;
                    mirrorEditedPartyMember();
                }
                return;
            }

            // Touch: close button (99), or a stat row (0-5) / shiny (6) -> select + act (like A).
            // Close DISCARDS unsaved edits -- restoreEditTarget() rolls the mon back to the last
            // Save/open snapshot; the individual Save button (X) is the only commit. The creator is the
            // exception: its not-yet-committed mon asks Keep/Discard instead.
            if (tb == 99) {
                if (creator.editing) { creator.keepConfirmActive = true; creator.keepConfirmIndex = 1; return; }
                restoreEditTarget();
                closeDetailsModal();
                return;
            }
            if (tb >= 0 && tb <= 31) { details.selectedField = tb; kDown |= HidNpadButton_A; }

            // B closes the page, DISCARDING any unsaved edits (rolled back to the last Save/open snapshot).
            if (kDown & HidNpadButton_B) {
                if (creator.editing) { creator.keepConfirmActive = true; creator.keepConfirmIndex = 1; return; }
                restoreEditTarget();
                closeDetailsModal();
                return;
            }

            // Three-column navigation: Up/Down cycle within the current column; Left/Right hop between the
            // center column (0-9: stats/shiny/nature/gender/level), the moves column (10-14, right panel),
            // and the editable detail column (left panel; ids 15-25, navigated in draw order).
            {
                int& f = details.selectedField;
                // Left-pane editable field ids in DRAW (top-to-bottom) order, built by the modal draw
                // (PokemonDetailsModal.cpp) so conditional rows (Form, Stat Nature, Pokerus, ...) appear
                // only when actually shown and the cursor visits exactly what's on screen. Fall back to
                // the always-present rows on the very first frame, before the draw has run once.
                std::vector<int> L = details.leftOrder;
                if (L.empty()) L = { 15, 16, 17, 18, 25, 19, 20, 21 };
                const bool inRight = (f >= 10 && f <= 14);
                const bool inLeft  = (f >= 15);
                auto leftMove = [&](int dir) -> int {
                    int pos = 0;
                    for (int i = 0; i < static_cast<int>(L.size()); ++i) if (L[i] == f) { pos = i; break; }
                    return L[(pos + dir + static_cast<int>(L.size())) % static_cast<int>(L.size())];
                };
                if (kDown & HidNpadButton_Up)
                    f = inRight ? 10 + ((f - 10 - 1 + 5) % 5)
                      : inLeft  ? leftMove(-1)
                      :           (f - 1 + 10) % 10;
                if (kDown & HidNpadButton_Down)
                    f = inRight ? 10 + ((f - 10 + 1) % 5)
                      : inLeft  ? leftMove(+1)
                      :           (f + 1) % 10;
                if (kDown & HidNpadButton_Right) {
                    if (inLeft)        f = details.lastCenterField;               // left  -> center
                    else if (!inRight) { details.lastCenterField = f; f = 10; }   // center -> right
                }
                if (kDown & HidNpadButton_Left) {
                    if (inRight)       f = details.lastCenterField;               // right -> center
                    else if (!inLeft)  { details.lastCenterField = f; f = L.front(); }  // center -> left
                }
            }

            // A on an editable row opens the right editor: stat dialog (0-5), shiny toggle (6), or the
            // selection picker (7 nature, 8 gender, 9-12 the four move slots).
            if ((kDown & HidNpadButton_A) && pokemon) {
                const int f = details.selectedField;
                if (f == 6) {
                    bool currentShiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
                    pokemon->setShiny(!currentShiny, pokemon->id32());
                    hasUnsavedChanges = true;
                    mirrorEditedPartyMember();  // keep an LGPE party member's box/party copies in sync
                } else if (f == 7) {          // Nature
                    pickerKind = Dialogs::PickerKind::Nature;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->nature();
                    pickerActive = true;
                } else if (f == 8) {          // Gender
                    pickerKind = Dialogs::PickerKind::Gender;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->gender();
                    pickerActive = true;
                } else if (f == 9) {          // Level (1-100 picker; box mons derive the level from EXP)
                    pickerKind = Dialogs::PickerKind::Level;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    uint8_t lvl = pokemon->level();
                    if (lvl == 0) lvl = Pokemon::getLevelFromExp(pokemon->exp(), Pokemon::getGrowthRate(pokemon->speciesID()));
                    pickerSel = (lvl > 0 ? lvl : 1) - 1;
                    pickerActive = true;
                } else if (f >= 10 && f <= 13) {  // Move slots 0-3
                    pickerKind = Dialogs::PickerKind::Move;
                    pickerSlot = f - 10;
                    // learnable moves first (green + top), for the mon's own game. Illegal moves are
                    // still offered (after the legal ones) and flagged by the legality check, PKHeX-style.
                    buildMovePickerOrder(pokemon->speciesID(), pokemon->form(), pokemon->getGameGroup(), pokemon->move(pickerSlot));
                    pickerCount = static_cast<int>(pickerOrder.size());
                    pickerActive = true;
                } else if (f == 14) {            // Held item
                    // Gen 3 held items are a separate, much smaller id space -- offering the modern
                    // list would both misname every entry and let an id be set that Gen 3 has no
                    // item for.
                    pickerKind = (pokemon->getGameGroup() == GameVersion::FRLG)
                               ? Dialogs::PickerKind::ItemG3 : Dialogs::PickerKind::Item;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->heldItem();
                    pickerActive = true;
                } else if (f == 15) {            // Ability
                    pickerKind = Dialogs::PickerKind::Ability;
                    // legal abilities first (green + top); sets pickerOrder, pickerLegalCount, pickerSel.
                    buildAbilityPickerOrder(pokemon->speciesID(), pokemon->form(), pokemon->ability());
                    pickerCount = static_cast<int>(pickerOrder.size());
                    pickerActive = true;
                } else if (f == 16) {            // Friendship
                    pickerKind = Dialogs::PickerKind::Friendship;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->friendship();
                    pickerActive = true;
                } else if (f == 26) {            // Form (species alternate forms)
                    pickerKind = Dialogs::PickerKind::Form;
                    pickerFormSpecies = pokemon->speciesID();
                    pickerCount = Pokemon::getPersonalInfo(pokemon->speciesID(), 0).formCount;
                    if (pickerCount < 1) pickerCount = 1;
                    pickerSel = pokemon->form();
                    if (pickerSel >= pickerCount) pickerSel = 0;
                    pickerActive = true;
                } else if (f == 27) {            // Stat Nature (mint / effective-stat nature)
                    pickerKind = Dialogs::PickerKind::StatNature;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->statNature();
                    if (pickerSel >= 25) pickerSel = pokemon->nature();
                    pickerActive = true;
                } else if (f == 23) {            // Nickname (blocking text keyboard)
                    std::string cur = Utils::utf16ToUtf8(pokemon->nickname());
                    Utils::KeyboardResult kr = Utils::promptText("昵称", "昵称", cur, 12);
                    if (kr.accepted) {
                        if (!kr.text.empty()) {
                            pokemon->setNickname(Utils::utf8ToUtf16(kr.text));
                            pokemon->setIsNicknamed(true);   // deliberate custom name
                        } else {
                            // An empty entry resets to the species default name (not nicknamed).
                            pokemon->setNickname(Utils::utf8ToUtf16(pokemon->species()));
                            pokemon->setIsNicknamed(false);
                        }
                        hasUnsavedChanges = true;
                        mirrorEditedPartyMember();
                    }
                } else if (f == 24) {            // EXP (blocking numeric keypad)
                    uint32_t maxExp = Pokemon::getExpForLevel(100, Pokemon::getGrowthRate(pokemon->speciesID()));
                    Utils::NumberResult nr = Utils::promptNumber("经验值", static_cast<int>(pokemon->exp()),
                                                                 0, static_cast<int>(maxExp));
                    if (nr.accepted) {
                        pokemon->setExp(static_cast<uint32_t>(nr.value));
                        hasUnsavedChanges = true;
                        mirrorEditedPartyMember();
                    }
                } else if (f == 28) {            // Met Date -- ONE numeric keypad (YYYYMMDD).
                    // Chaining several swkbd launches inside a single frame crashes on hardware (a library
                    // applet can't be relaunched back-to-back), so the whole date is one field.
                    int cur = (2000 + pokemon->metYear()) * 10000
                            + (pokemon->metMonth() ? pokemon->metMonth() : 1) * 100
                            + (pokemon->metDay() ? pokemon->metDay() : 1);
                    Utils::NumberResult r = Utils::promptNumber("相遇日期（YYYYMMDD）", cur, 20000101, 20991231);
                    if (r.accepted) {
                        int yy = r.value / 10000, mm = (r.value / 100) % 100, dd = r.value % 100;
                        if (mm >= 1 && mm <= 12 && dd >= 1 && dd <= 31) {
                            pokemon->setMetYear(static_cast<uint8_t>((yy - 2000) & 0xFF));
                            pokemon->setMetMonth(static_cast<uint8_t>(mm));
                            pokemon->setMetDay(static_cast<uint8_t>(dd));
                            hasUnsavedChanges = true; mirrorEditedPartyMember();
                        } else {
                            postStatus("日期无效，请使用 YYYYMMDD 格式。", 200);
                        }
                    }
                } else if (f == 29) {            // Egg Location (Met-location picker routed to the egg field)
                    const uint8_t ver = pokemon->originGame();
                    Names::LocationTable lt = Names::getLocationTable(ver);
                    pickerOrder.clear();
                    for (uint16_t id = 0; id < lt.count; ++id)
                        if (lt.names[id][0] != '\0') pickerOrder.push_back(id);
                    if (!pickerOrder.empty()) {
                        pickerMetVersion = ver;
                        pickerMetIsEgg = true;   // this picker targets the egg-met location
                        pickerKind = Dialogs::PickerKind::MetLocation;
                        pickerCount = static_cast<int>(pickerOrder.size());
                        pickerSel = 0;
                        for (int i = 0; i < static_cast<int>(pickerOrder.size()); ++i)
                            if (pickerOrder[i] == pokemon->eggLocation()) { pickerSel = i; break; }
                        pickerActive = true;
                    } else {
                        postStatus("此游戏没有可用的相遇地点列表。", 200);
                    }
                } else if (f == 30) {            // Egg Date -- ONE numeric keypad (YYYYMMDD), see Met Date.
                    int cur = (2000 + pokemon->eggYear()) * 10000
                            + (pokemon->eggMonth() ? pokemon->eggMonth() : 1) * 100
                            + (pokemon->eggDay() ? pokemon->eggDay() : 1);
                    Utils::NumberResult r = Utils::promptNumber("蛋获得日期（YYYYMMDD）", cur, 20000101, 20991231);
                    if (r.accepted) {
                        int yy = r.value / 10000, mm = (r.value / 100) % 100, dd = r.value % 100;
                        if (mm >= 1 && mm <= 12 && dd >= 1 && dd <= 31) {
                            pokemon->setEggYear(static_cast<uint8_t>((yy - 2000) & 0xFF));
                            pokemon->setEggMonth(static_cast<uint8_t>(mm));
                            pokemon->setEggDay(static_cast<uint8_t>(dd));
                            hasUnsavedChanges = true; mirrorEditedPartyMember();
                        } else {
                            postStatus("日期无效，请使用 YYYYMMDD 格式。", 200);
                        }
                    }
                } else if (f == 31) {            // Fateful Encounter (toggle)
                    pokemon->setFatefulEncounter(!pokemon->isFatefulEncounter());
                    hasUnsavedChanges = true;
                    mirrorEditedPartyMember();
                } else if (f == 17) {            // Egg (toggle not-egg <-> egg)
                    pokemon->setEgg(!pokemon->isEgg());
                    hasUnsavedChanges = true;
                    mirrorEditedPartyMember();
                } else if (f == 18) {            // Met Level (1-100 picker)
                    pickerKind = Dialogs::PickerKind::MetLevel;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = (pokemon->metLevel() > 0 ? pokemon->metLevel() : 1) - 1;
                    pickerActive = true;
                } else if (f == 19) {            // Ball (per-game list -- PLA uses the Hisui balls)
                    pickerKind = Dialogs::PickerKind::Ball;
                    pickerOrder.clear();
                    for (uint8_t b : Enums::getBallList(pokemon->getGameGroup())) pickerOrder.push_back(b);
                    pickerLegalCount = 0;   // no green "合法" prefix for balls
                    pickerCount = static_cast<int>(pickerOrder.size());
                    pickerSel = 0;          // land on the mon's current ball if it's in the list
                    for (int i = 0; i < static_cast<int>(pickerOrder.size()); ++i)
                        if (pickerOrder[i] == pokemon->ball()) { pickerSel = i; break; }
                    pickerActive = true;
                } else if (f == 20) {            // Language
                    pickerKind = Dialogs::PickerKind::Language;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->language();
                    pickerActive = true;
                } else if (f == 21) {            // Origin game
                    pickerKind = Dialogs::PickerKind::Origin;
                    pickerCount = Dialogs::pickerOptionCount(pickerKind);
                    pickerSel = pokemon->originGame();
                    pickerActive = true;
                } else if (f == 22 && pokemon->hasPokerus()) {   // Pokerus: cycle None -> Infected -> Cured
                    uint8_t next;
                    if (pokemon->isPokerusInfected())   next = 0x10;  // Infected -> Cured (strain 1, 0 days)
                    else if (pokemon->isPokerusCured()) next = 0x00;  // Cured -> None
                    else                                next = 0x12;  // None -> Infected (strain 1, 2 days)
                    pokemon->setPokerus(next);
                    hasUnsavedChanges = true;
                    mirrorEditedPartyMember();
                } else if (f == 25) {            // Met Location (picker of the origin game's places)
                    const uint8_t ver = pokemon->originGame();
                    Names::LocationTable lt = Names::getLocationTable(ver);
                    pickerOrder.clear();
                    for (uint16_t id = 0; id < lt.count; ++id)
                        if (lt.names[id][0] != '\0') pickerOrder.push_back(id);   // real, non-blank places only
                    if (!pickerOrder.empty()) {
                        pickerMetVersion = ver;
                        pickerMetIsEgg = false;   // this picker targets the met location
                        pickerKind = Dialogs::PickerKind::MetLocation;
                        pickerCount = static_cast<int>(pickerOrder.size());
                        pickerSel = 0;   // land on the current location if it's in the list
                        for (int i = 0; i < static_cast<int>(pickerOrder.size()); ++i)
                            if (pickerOrder[i] == pokemon->metLocation()) { pickerSel = i; break; }
                        pickerActive = true;
                    } else {
                        postStatus("此游戏没有可用的相遇地点列表。", 200);
                    }
                } else if (f >= 15) {
                    // A left-column detail row with no editor (e.g. Pokerus on a game without the
                    // mechanic) -- do nothing rather than fall through to the stat editor below.
                } else {
                    const int st = f;  // 0-5 (HP, Atk, Def, SpA, SpD, Spe)
                    statEdit.selectedStat = st;
                    switch (st) {
                        case 0: statEdit.originalIV = pokemon->ivHP();  statEdit.originalEV = pokemon->evHP();  statEdit.originalAV = pokemon->avHP();  break;
                        case 1: statEdit.originalIV = pokemon->ivATK(); statEdit.originalEV = pokemon->evATK(); statEdit.originalAV = pokemon->avATK(); break;
                        case 2: statEdit.originalIV = pokemon->ivDEF(); statEdit.originalEV = pokemon->evDEF(); statEdit.originalAV = pokemon->avDEF(); break;
                        case 3: statEdit.originalIV = pokemon->ivSPA(); statEdit.originalEV = pokemon->evSPA(); statEdit.originalAV = pokemon->avSPA(); break;
                        case 4: statEdit.originalIV = pokemon->ivSPD(); statEdit.originalEV = pokemon->evSPD(); statEdit.originalAV = pokemon->avSPD(); break;
                        case 5: statEdit.originalIV = pokemon->ivSPE(); statEdit.originalEV = pokemon->evSPE(); statEdit.originalAV = pokemon->avSPE(); break;
                    }
                    statEdit.currentIV = statEdit.originalIV;
                    statEdit.currentEV = statEdit.originalEV;
                    statEdit.currentAV = statEdit.originalAV;
                    statEdit.mode = Dialogs::StatEditMode::IV;
                    statEdit.value = statEdit.currentIV;
                    statEdit.dialogActive = true;
                }
            }

            return;  // Don't process other inputs while the page is active
        }

        // Handle edit dialog
        if (itemEditDialogActive) {
            // Touch: map the step buttons to the SAME presses as the stat editor
            // (ZL/L/Left/Right/R/ZR = -/+100, -/+10, -/+1), Confirm -> A, Cancel -> B, then reuse the
            // button logic below.
            switch (touchedButtonId(touch)) {
                case 34: kDown |= HidNpadButton_ZL;    break;
                case 30: kDown |= HidNpadButton_L;     break;
                case 31: kDown |= HidNpadButton_Left;  break;
                case 32: kDown |= HidNpadButton_Right; break;
                case 33: kDown |= HidNpadButton_R;     break;
                case 35: kDown |= HidNpadButton_ZR;    break;
                case 1:  kDown |= HidNpadButton_A;     break;
                case 0:  kDown |= HidNpadButton_B;     break;
                case 40: kDown |= HidNpadButton_X;     break;  // item drop-down -> change type
                case 2:  kDown |= HidNpadButton_Y;     break;  // Remove
                default: break;
            }
            // Adjust value, bounded by what the GAME accepts for this pouch. Over-cap counts are
            // a save-corruption vector, not cosmetic: an oversized Gen 3 stack overflows the bag into
            // the key-items pocket. Left/Right = -/+1, L/R = -/+10, ZL/ZR = -/+100 -- matching the stat
            // editor (these used to be Up/Down, inconsistent between the two dialogs).
            const int itemMax = currentItemMaxCount();
            if (kDown & HidNpadButton_Left)  itemEditDialogValue = std::max(0, itemEditDialogValue - 1);
            if (kDown & HidNpadButton_Right) itemEditDialogValue = std::min(itemMax, itemEditDialogValue + 1);
            if (kDown & HidNpadButton_L)     itemEditDialogValue = std::max(0, itemEditDialogValue - 10);
            if (kDown & HidNpadButton_R)     itemEditDialogValue = std::min(itemMax, itemEditDialogValue + 10);
            if (kDown & HidNpadButton_ZL)    itemEditDialogValue = std::max(0, itemEditDialogValue - 100);
            if (kDown & HidNpadButton_ZR)    itemEditDialogValue = std::min(itemMax, itemEditDialogValue + 100);
            // A save edited elsewhere (or by an older PKSE) can hold an over-cap count; clamp on
            // entry so opening the dialog can only ever move it back into range, never past it.
            itemEditDialogValue = std::clamp(itemEditDialogValue, 0, itemMax);

            // X: change this item's TYPE. Open the pouch picker filtered to legal ids the bag doesn't
            // already hold; the pick reassigns this slot (see the picker's replace-confirm handler).
            if (kDown & HidNpadButton_X) {
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    const auto& pouch = trainer.items[selectedCategory];
                    const auto legal = Names::getPouchItems(trainer.getGameGroup(), selectedCategory);
                    pickerOrder.clear();
                    for (uint16_t id : legal) {
                        const auto e = std::find_if(pouch.begin(), pouch.end(),
                            [id](const Trainer::InventoryItem& v) { return v.itemId == id; });
                        if (e == pouch.end() || e->count == 0) pickerOrder.push_back(id);   // not currently held
                    }
                    if (!pickerOrder.empty()) {
                        itemPickerReplace = true;
                        pickerKind = (trainer.getGameGroup() == GameVersion::FRLG)
                                   ? Dialogs::PickerKind::PouchItemG3 : Dialogs::PickerKind::PouchItem;
                        pickerCount = static_cast<int>(pickerOrder.size());
                        pickerSel = 0;
                        pickerActive = true;
                        itemEditDialogActive = false;   // picker takes over; the change applies on pick
                    } else {
                        postStatus("此口袋中没有其他可用道具。", 240);
                    }
                }
                return;
            }

            // Y: remove this item from the pouch.
            if (kDown & HidNpadButton_Y) {
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    auto& pouch = trainer.items[selectedCategory];
                    std::vector<int> visible = visibleItemIndices();
                    if (selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(visible.size())) {
                        const int rawIdx = visible[selectedItemIndex];
                        if (trainer.itemsAreIdIndexed()) {
                            pouch[rawIdx].count = 0;   // keep the entry so its id-slot is written to 0
                        } else {
                            pouch.erase(pouch.begin() + rawIdx);   // slot-based: erase; region rewritten
                        }
                        hasUnsavedChanges = true;
                        const int vis = static_cast<int>(visibleItemIndices().size());
                        if (selectedItemIndex >= vis) selectedItemIndex = std::max(0, vis - 1);
                        postStatus("道具已移除。", 200);
                    }
                }
                itemEditDialogActive = false;
                return;
            }

            // Confirm edit
            if (kDown & HidNpadButton_A) {
                // Save the new value to the item (map the visible selection to the raw pouch slot).
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    auto& pouch = trainer.items[selectedCategory];
                    std::vector<int> visible = visibleItemIndices();
                    if (selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(visible.size())) {
                        const int rawIdx = visible[selectedItemIndex];
                        // Confirming a count of 0 IS a removal. On a slot-based game erase the entry, or
                        // updateItemBlock would write a {itemId, 0} ghost slot; on an id-indexed game
                        // keep the entry at count 0 so its id-slot is written to 0.
                        if (itemEditDialogValue == 0 && !trainer.itemsAreIdIndexed()) {
                            pouch.erase(pouch.begin() + rawIdx);
                        } else {
                            pouch[rawIdx].count = static_cast<uint16_t>(itemEditDialogValue);
                        }
                        if (itemEditDialogValue != itemEditDialogOriginalValue) hasUnsavedChanges = true;
                    }
                }
                itemEditDialogActive = false;
                return;
            }

            // Cancel edit
            if (kDown & HidNpadButton_B) {
                itemEditDialogActive = false;
                return;
            }

            return;  // Don't process other inputs while edit dialog is active
        }

        // Handle storage release confirmation (single slot or the whole multi-selection).
        if (releaseConfirmActive) {
            int tb = touchedButtonId(touch);
            if (tb == 1) kDown |= HidNpadButton_A;       // tap Release
            else if (tb == 0) kDown |= HidNpadButton_B;  // tap Cancel
            if (kDown & HidNpadButton_A) {
                if (releaseGroup) {
                    for (const auto& s : multiSel) storageSlot(s.pane, s.box, s.slot).reset();
                    multiSel.clear();
                } else {
                    storageSlot(releasePane, releaseBox, releaseSlot).reset();
                }
                hasUnsavedChanges = true;
                releaseConfirmActive = false;
                return;
            }
            if (kDown & HidNpadButton_B) { releaseConfirmActive = false; return; }
            return;
        }

        // Item removal confirm: X in the Items list asks before deleting. A = Remove, B = Cancel.
        // Mirrors the storage release confirm; the delete matches the Edit Item dialog's Y-remove.
        if (itemRemoveConfirmActive) {
            int tb = touchedButtonId(touch);
            if (tb == 1) kDown |= HidNpadButton_A;       // tap Remove
            else if (tb == 0) kDown |= HidNpadButton_B;  // tap Cancel
            if (kDown & HidNpadButton_A) {
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    auto& pouch = trainer.items[selectedCategory];
                    std::vector<int> visible = visibleItemIndices();
                    if (selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(visible.size())) {
                        const int rawIdx = visible[selectedItemIndex];
                        if (trainer.itemsAreIdIndexed()) pouch[rawIdx].count = 0;   // keep entry; id-slot written to 0
                        else pouch.erase(pouch.begin() + rawIdx);                   // slot-based: erase; region rewritten
                        hasUnsavedChanges = true;
                        const int vis = static_cast<int>(visibleItemIndices().size());
                        if (selectedItemIndex >= vis) selectedItemIndex = std::max(0, vis - 1);
                        postStatus("道具已移除。", 200);
                    }
                }
                itemRemoveConfirmActive = false;
                return;
            }
            if (kDown & HidNpadButton_B) { itemRemoveConfirmActive = false; return; }
            return;
        }

        // Let's Go move acknowledgement: converting to/from LGPE resets AVs/EVs. Continue runs the
        // stashed action (single place / group move / group transfer); Cancel leaves everything as-is.
        if (lgpeMoveConfirmActive) {
            int tb = touchedButtonId(touch);
            if (tb == 1) kDown |= HidNpadButton_A;        // tap Continue
            else if (tb == 0) kDown |= HidNpadButton_B;   // tap Cancel
            if (kDown & HidNpadButton_B) { lgpeMoveConfirmActive = false; lgpePending = LgpePending::None; return; }
            if (kDown & HidNpadButton_A) {   // Continue: run the stashed action (A always continues)
                lgpeMoveConfirmActive = false;
                switch (lgpePending) {
                    case LgpePending::PlaceHeld: {
                        const int p = lgpePendPane, b = lgpePendBox, s = lgpePendSlot;
                        if (!storageSlotLocked(p, b, s) && prepareHeldForPane(p)) {
                            auto& here = storageSlot(p, b, s);
                            if (!here || here->speciesID() == 0) here = std::move(heldPokemon);
                            else { std::swap(here, heldPokemon); heldPane = p; heldFromBox = b; heldFromSlot = s; }
                            hasUnsavedChanges = true;
                        }
                        break;
                    }
                    case LgpePending::GroupMoveTo:   moveSelectionTo(lgpePendPane, lgpePendBox, lgpePendSlot); break;
                    case LgpePending::GroupTransfer: transferSelectionToOtherPane(); break;
                    default: break;
                }
                lgpePending = LgpePending::None;
                return;
            }
            return;
        }

        // Handle the red-mode per-Pokemon action menu (Move / Edit / Release / Cancel).
        if (storageMenuActive) {
            constexpr int N = 5;
            int tb = touchedButtonId(touch);
            if (tb >= 0) { storageMenuIndex = tb; kDown |= HidNpadButton_A; }  // tap a row = select + confirm
            // On a party-linked (locked) slot, Move (0) and Release (3) are disabled -- skip them so
            // Up/Down never land on a greyed row, and guard the actions below in case a tap slips in.
            const bool menuLocked = storageSlotLocked(menuPane, menuBox, menuSlot);
            auto menuDisabled = [&](int i) { return menuLocked && (i == 0 || i == 3); };
            if (kDown & HidNpadButton_Up)   do { storageMenuIndex = (storageMenuIndex - 1 + N) % N; } while (menuDisabled(storageMenuIndex));
            if (kDown & HidNpadButton_Down) do { storageMenuIndex = (storageMenuIndex + 1) % N; } while (menuDisabled(storageMenuIndex));
            if (kDown & HidNpadButton_B) { storageMenuActive = false; return; }
            if (kDown & HidNpadButton_A) {
                storageMenuActive = false;
                switch (storageMenuIndex) {
                    case 0:  // Move -> pick up (blocked on a party-linked slot)
                        if (menuLocked) break;
                        heldPokemon = std::move(storageSlot(menuPane, menuBox, menuSlot));
                        heldPane = menuPane; heldFromBox = menuBox; heldFromSlot = menuSlot;
                        break;
                    case 1:  // Edit -> open the details modal
                        openStorageEditor(menuPane, menuBox, menuSlot);
                        break;
                    case 2:  // Clone -> duplicate into the first empty slot (this box first, then ANY box)
                        if (const Pokemon::Pokemon* src = storageSlot(menuPane, menuBox, menuSlot).get()) {
                            if (auto copy = src->clone()) {
                                const int slots = menuPane == 0 ? static_cast<int>(trainer.getSlotsPerBox())
                                                                : static_cast<int>(Trainer::Bank::BANK_SLOTS_PER_BOX);
                                const int boxes = menuPane == 0 ? static_cast<int>(trainer.getBoxCount())
                                                                : static_cast<int>(Trainer::Bank::BANK_BOX_COUNT);
                                // Search the mon's own box first, then spill into any other box. A full
                                // box must NOT eat the clone -- on Let's Go's gapless list every box but
                                // the last is full, so a single-box search silently dropped the copy (a
                                // party mon in box 0 could never be cloned).
                                bool placed = false;
                                for (int bo = 0; bo < boxes && !placed; ++bo) {
                                    const int box = (menuBox + bo) % boxes;
                                    for (int s = 0; s < slots && !placed; ++s) {
                                        auto& dst = storageSlot(menuPane, box, s);
                                        if ((!dst || dst->speciesID() == 0) && !storageSlotLocked(menuPane, box, s)) {
                                            dst = std::move(copy);
                                            hasUnsavedChanges = true;
                                            placed = true;
                                        }
                                    }
                                }
                                if (!placed) postStatus("没有可用于复制的空槽位。", 240);
                            }
                        }
                        break;
                    case 3:  // Release -> confirm (blocked on a party-linked slot)
                        if (menuLocked) break;
                        releaseGroup = false;
                        releasePane = menuPane; releaseBox = menuBox; releaseSlot = menuSlot;
                        releaseConfirmActive = true;
                        break;
                    default: break;  // Cancel
                }
                return;
            }
            return;
        }

        // Handle the green-mode group menu (Move / Release / Clear / Cancel).
        if (groupMenuActive) {
            constexpr int N = 4;
            int tb = touchedButtonId(touch);
            if (tb >= 0) { groupMenuIndex = tb; kDown |= HidNpadButton_A; }  // tap a row = select + confirm
            if (kDown & HidNpadButton_Up)   groupMenuIndex = (groupMenuIndex - 1 + N) % N;
            if (kDown & HidNpadButton_Down) groupMenuIndex = (groupMenuIndex + 1) % N;
            if (kDown & HidNpadButton_B) { groupMenuActive = false; return; }
            if (kDown & HidNpadButton_A) {
                groupMenuActive = false;
                switch (groupMenuIndex) {
                    case 0: {  // Move to the other pane
                        const int destPane = multiSel.empty() ? 1 : (1 - multiSel.front().pane);
                        const bool g3warn = selectionInvolvesGen3Downgrade(destPane);
                        const bool lgwarn = g_lgpeMoveWarn && selectionInvolvesLgpe(destPane);
                        if (g3warn || lgwarn) {   // confirm a lossy conversion (Gen 3 PID rebuild / LGPE AV-EV reset)
                            lgpePending = LgpePending::GroupTransfer;
                            moveConfirmGen3 = g3warn; lgpeMoveConfirmActive = true; lgpeMoveConfirmIndex = 1;
                        } else {
                            transferSelectionToOtherPane();
                        }
                        break;
                    }
                    case 1: releaseGroup = true; releaseConfirmActive = true; break;  // Release all
                    case 2: multiSel.clear(); break;  // Clear selection
                    default: break;  // Cancel
                }
                return;
            }
            return;
        }

        // Handle the storage-exit Save / Discard / Cancel prompt (bank has unsaved changes).
        if (storageExitConfirmActive) {
            constexpr int N = 3;
            int tb = touchedButtonId(touch);
            if (tb >= 0) { storageExitConfirmIndex = tb; kDown |= HidNpadButton_A; }  // tap a row = select + confirm
            if (kDown & HidNpadButton_Up)   storageExitConfirmIndex = (storageExitConfirmIndex - 1 + N) % N;
            if (kDown & HidNpadButton_Down) storageExitConfirmIndex = (storageExitConfirmIndex + 1) % N;
            // Cancel: stay, and call off any app exit that raised this (they backed out of leaving too).
            if (kDown & HidNpadButton_B) {
                storageExitConfirmActive = false;
                exitAfterBankChoice = false;
                return;
            }
            if (kDown & HidNpadButton_A) {
                storageExitConfirmActive = false;
                // Claimed up front, so a branch that bails out (a failed bank write) can't leave a
                // stale "and then exit" hanging over the next visit to this prompt.
                const bool resumeExit = exitAfterBankChoice;
                exitAfterBankChoice = false;
                bool answered = false;   // Save or Discard actually went through (Cancel did not)
                switch (storageExitConfirmIndex) {
                    case 0:  // Save & Exit
                        // Same rule as the app-exit path: a failed bank write must not be followed by
                        // leaving the view, or the deposits vanish with no indication.
                        if (bank && !bank->save()) {
                            postStatus("无法保存银行。为防止数据丢失，将停留在存储界面。", 480);
                            return;
                        }
                        Utils::logTest("BANKSAVE result=OK verifyfail=" +
                                       std::to_string(bank ? bank->lastVerifyFailures() : 0));
                        // Written, but the round-trip check found slots that don't reproduce. That
                        // is a PKSE bug rather than a write failure, so it warns instead of blocking.
                        if (bank && bank->lastVerifyFailures() > 0) {
                            postStatus(std::to_string(bank->lastVerifyFailures()) +
                                       " 个银行槽位未通过完整性检查，请查看 PKSE 日志。", 480);
                        }
                        detailViewActive = false;
                        answered = true;
                        break;
                    case 1:  // Discard & Exit -> revert the in-memory bank to its on-disk state.
                             // ONLY the bank: it owns what lives in the bank, not what lives in the
                             // save. Pulling a Pokemon out and then discarding therefore leaves the
                             // copy in the save box -- intended, and what PKSM does. Undoing that half
                             // is the GAME save's own discard, which is a separate decision.
                        if (bank) bank->load();
                        detailViewActive = false;
                        answered = true;
                        break;
                    default: break;  // Cancel: stay in the storage view
                }
                // Raised by + rather than by B: the bank has had its answer, so carry on out of the
                // app. Cancel leaves `answered` false and simply stays.
                if (resumeExit && answered) beginAppExit();
                return;
            }
            return;
        }

        // Handle detail view exit
        if (detailViewActive) {
            if (kDown & HidNpadButton_B) {
                if (selectedMode == ViewMode::Storage) {
                    // B unwinds one step: drop a carried Pokemon, then clear a multi-selection, then exit.
                    if (heldPokemon) { returnHeldToOrigin(); return; }
                    if (!multiSel.empty()) { multiSel.clear(); return; }
                    // Leaving the storage view: if the bank has unsaved changes, prompt Save / Discard /
                    // Cancel (PKSM does the same on its storage back button). No changes -> just exit.
                    // The bank is its own entity, saved here rather than with the game (X) save.
                    if (bank && bank->hasChanged()) {
                        storageExitConfirmActive = true;
                        storageExitConfirmIndex = 0;
                        return;
                    }
                    detailViewActive = false;
                    return;
                }
                if (swapActive) {
                    swapActive = false;  // cancel an in-progress grab rather than leaving the grid
                    return;
                }
                // Exit detail view (B button only)
                detailViewActive = false;
                selectedItemIndex = 0;
                currentPage = 0;
                return;
            }

            // Storage view: dual-pane bank navigation + deposit/withdraw.
            if (selectedMode == ViewMode::Storage) {
                // Touch: a tap on a slot moves the cursor there and acts like pressing A, so it
                // works in every cursor mode (Menu opens the popup, Move picks up/places, Multi
                // toggles/moves). Rects were captured during the previous frame's draw.
                if (touch.justPressed()) {
                    for (const auto& t : storageTouchTargets) {
                        if (touch.x() >= t.x && touch.x() < t.x + t.w &&
                            touch.y() >= t.y && touch.y() < t.y + t.h) {
                            storageFocusPane = t.pane;
                            if (t.slot == -2)      kDown |= HidNpadButton_L;  // ◀ prev box
                            else if (t.slot == -3) kDown |= HidNpadButton_R;  // ▶ next box
                            else if (t.slot == -4) {                          // tapped the name pill -> rename
                                if (t.pane == 1 || trainer.supportsBoxNames()) {
                                    if (t.pane == 0) stSaveSlot = -1; else stBankSlot = -1;  // focus header now
                                    pendingHeaderRename = true;               // open the rename next frame
                                }
                            }
                            else {
                                if (t.pane == 0) stSaveSlot = t.slot; else stBankSlot = t.slot;
                                kDown |= HidNpadButton_A;
                            }
                            break;
                        }
                    }
                }
                handleStorageInput(kDown);
                return;
            }

            // Handle detail view navigation
            if (selectedMode == ViewMode::Items) {
                // Get current pouch size for bounds checking
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    const auto& pouch = trainer.items[selectedCategory];
                    // Navigate/edit only the visible items (count > 0); selectedItemIndex indexes
                    // this filtered view, mapped back to the raw pouch via visible[...] on edit.
                    std::vector<int> visible = visibleItemIndices();
                    // Calculate items per column based on panel height
                    int itemsPerPage = (CONTENT_PANEL_HEIGHT - 106) / 52;  // rows that fit (mirror ItemsPanel)
                    if (itemsPerPage < 1) itemsPerPage = 1;
                    int totalItems = static_cast<int>(visible.size());
                    int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
                    // Clamp BOTH the selection and the page to the current item set. Removing every
                    // item on the last page shrinks totalItems/totalPages; without re-clamping here the
                    // cursor is stranded on an out-of-range page ("6/5") until the pouch/tab changes or
                    // the view is re-entered (E8). Re-derive currentPage from the clamped selection so
                    // the two stay consistent no matter how the item left (list remove, dialog remove,
                    // or an edit to count 0).
                    if (totalItems > 0) {
                        if (selectedItemIndex >= totalItems) selectedItemIndex = totalItems - 1;
                        if (selectedItemIndex < 0) selectedItemIndex = 0;
                        currentPage = selectedItemIndex / itemsPerPage;
                    } else {
                        selectedItemIndex = 0;
                        currentPage = 0;
                    }

                    // Touch: tap a row to select + edit it.
                    int itemTap = touchedButtonId(touch);
                    if (itemTap >= 0 && itemTap < totalItems) {
                        selectedItemIndex = itemTap;
                        currentPage = selectedItemIndex / itemsPerPage;
                        kDown |= HidNpadButton_A;
                    }

                    // Up/Down to select items
                    if ((kDown & HidNpadButton_Up) && totalItems > 0) {
                        selectedItemIndex = (selectedItemIndex - 1 + totalItems) % totalItems;
                        // Adjust page if needed
                        currentPage = selectedItemIndex / itemsPerPage;
                    }
                    if ((kDown & HidNpadButton_Down) && totalItems > 0) {
                        selectedItemIndex = (selectedItemIndex + 1) % totalItems;
                        // Adjust page if needed
                        currentPage = selectedItemIndex / itemsPerPage;
                    }

                    // Left/Right page through the single-column list.
                    if ((kDown & HidNpadButton_Right) && totalPages > 1) {
                        currentPage = (currentPage + 1) % totalPages;
                        selectedItemIndex = std::min(currentPage * itemsPerPage, totalItems - 1);
                    }
                    if ((kDown & HidNpadButton_Left) && totalPages > 1) {
                        currentPage = (currentPage - 1 + totalPages) % totalPages;
                        selectedItemIndex = std::min(currentPage * itemsPerPage, totalItems - 1);
                    }

                    // A button to edit item amount (map the visible selection back to the raw pouch slot)
                    if (kDown & HidNpadButton_A) {
                        if (selectedItemIndex >= 0 && selectedItemIndex < totalItems) {
                            int rawIdx = visible[selectedItemIndex];
                            // Open edit dialog for selected item
                            itemEditDialogActive = true;
                            itemEditDialogValue = pouch[rawIdx].count;
                            itemEditDialogOriginalValue = pouch[rawIdx].count;
                        }
                    }

                    // Y opens the add-item picker: the ids that legally belong in THIS pouch,
                    // minus what the bag already holds. Nothing is offered for a pouch the game
                    // doesn't have, or when appending isn't supported and every legal id is present.
                    if (kDown & HidNpadButton_Y) {
                        const auto legal = Names::getPouchItems(trainer.getGameGroup(), selectedCategory);
                        const bool canAppend = static_cast<int>(pouch.size()) < currentPouchCapacity();
                        pickerOrder.clear();
                        bool blockedByCapacity = false;   // a new item exists to add, but the pouch can't take it
                        for (uint16_t id : legal) {
                            const auto e = std::find_if(pouch.begin(), pouch.end(),
                                [id](const Trainer::InventoryItem& x) { return x.itemId == id; });
                            if (e != pouch.end()) {
                                if (e->count == 0) pickerOrder.push_back(id);   // present but empty -> settable
                            } else if (canAppend) {
                                pickerOrder.push_back(id);                      // genuinely new -> needs a slot
                            } else {
                                blockedByCapacity = true;
                            }
                        }
                        if (!pickerOrder.empty()) {
                            pickerKind = (trainer.getGameGroup() == GameVersion::FRLG)
                                       ? Dialogs::PickerKind::PouchItemG3
                                       : Dialogs::PickerKind::PouchItem;
                            pickerCount = static_cast<int>(pickerOrder.size());
                            pickerSel = 0;
                            pickerActive = true;
                        } else if (blockedByCapacity) {
                            // A slot-based pocket (FRLG / GG / SWSH / PLA) that's full -- say so rather
                            // than a silent no-op. The id-indexed games never hit this (entries preexist).
                            postStatus("此口袋已满。", 300);
                        } else {
                            postStatus("此口袋可容纳的道具已全部拥有。", 300);
                        }
                        return;
                    }

                    // X asks to remove the selected item, behind a confirm dialog. The delete
                    // runs in the itemRemoveConfirmActive handler on A; here we just open the prompt.
                    if ((kDown & HidNpadButton_X) && totalItems > 0
                        && selectedItemIndex >= 0 && selectedItemIndex < totalItems) {
                        itemRemoveConfirmActive = true;
                        return;
                    }

                    // L/R to change categories (but not when in edit dialog)
                    if (kDown & HidNpadButton_L) {
                        switch(trainer.getGameGroup()) {
                            case GameVersion::ZA: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_LZA) % POUCH_COUNT9_LZA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SV: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_SV) % POUCH_COUNT9_SV;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::PLA: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT8_LA) % POUCH_COUNT8_LA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::BDSP: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT8BDSP) % POUCH_COUNT8BDSP;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SWSH: {
                                selectedCategory = (selectedCategory - 1 + static_cast<int>(Trainer::PouchType8SWSH::Count)) % static_cast<int>(Trainer::PouchType8SWSH::Count);
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::GG: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT7_LGPE) % POUCH_COUNT7_LGPE;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::FRLG: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT3_FRLG) % POUCH_COUNT3_FRLG;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            default: break;
                        }
                    }
                    if (kDown & HidNpadButton_R) {
                        switch(trainer.getGameGroup()) {
                            case GameVersion::ZA: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_LZA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SV: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_SV;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::PLA: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT8_LA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::BDSP: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT8BDSP;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SWSH: {
                                selectedCategory = (selectedCategory + 1) % static_cast<int>(Trainer::PouchType8SWSH::Count);
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::GG: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT7_LGPE;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::FRLG: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT3_FRLG;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            default: break;
                        }
                    }
                }
            }

            // Handle detail view navigation for Boxes mode
            if (selectedMode == ViewMode::Boxes) {
                // Calculate grid layout based on game (LGPE: 5x5=25, others: 6x5=30)
                const int slotsPerBox = static_cast<int>(trainer.getSlotsPerBox());
                const int GRID_COLS = (slotsPerBox == 25) ? 5 : 6;  // LGPE uses 5x5, others use 6x5
                const int GRID_ROWS = 5;

                // selectedItemIndex == -1 is the "box-name header focused" state: navigate up to it
                // (or tap it) and press A to rename the box. Only where the game stores box names
                // (LGPE does not) and no swap is in progress; otherwise it keeps the top<->bottom wrap.
                const bool canFocusHeader = trainer.supportsBoxNames() && !swapActive;

                // Up/Down/Left/Right to navigate the grid (and to/from the header)
                if (kDown & HidNpadButton_Up) {
                    if (selectedItemIndex == -1) {
                        selectedItemIndex = (GRID_ROWS - 1) * GRID_COLS;          // header -> bottom row
                    } else {
                        int currentRow = selectedItemIndex / GRID_COLS;
                        int currentCol = selectedItemIndex % GRID_COLS;
                        if (currentRow > 0)      selectedItemIndex -= GRID_COLS;
                        else if (canFocusHeader) selectedItemIndex = -1;          // top row -> header
                        else                     selectedItemIndex = (GRID_ROWS - 1) * GRID_COLS + currentCol;
                    }
                }
                if (kDown & HidNpadButton_Down) {
                    if (selectedItemIndex == -1) {
                        selectedItemIndex = 0;                                     // header -> top-left
                    } else {
                        int currentRow = selectedItemIndex / GRID_COLS;
                        int currentCol = selectedItemIndex % GRID_COLS;
                        if (currentRow < GRID_ROWS - 1) selectedItemIndex += GRID_COLS;
                        else if (canFocusHeader)        selectedItemIndex = -1;    // bottom row -> header
                        else                            selectedItemIndex = currentCol;
                    }
                }
                if (kDown & HidNpadButton_Left) {
                    if (selectedItemIndex == -1) {                                // on the header: ◀ cycles box
                        int boxCount = static_cast<int>(trainer.getBoxCount());
                        selectedBoxIndex = (selectedBoxIndex - 1 + boxCount) % boxCount;
                    } else if (selectedItemIndex % GRID_COLS > 0) {
                        selectedItemIndex--;
                    } else {
                        int currentRow = selectedItemIndex / GRID_COLS;
                        selectedItemIndex = currentRow * GRID_COLS + (GRID_COLS - 1);
                    }
                }
                if (kDown & HidNpadButton_Right) {
                    if (selectedItemIndex == -1) {                                // on the header: ▶ cycles box
                        int boxCount = static_cast<int>(trainer.getBoxCount());
                        selectedBoxIndex = (selectedBoxIndex + 1) % boxCount;
                    } else if (selectedItemIndex % GRID_COLS < GRID_COLS - 1) {
                        selectedItemIndex++;
                    } else {
                        int currentRow = selectedItemIndex / GRID_COLS;
                        selectedItemIndex = currentRow * GRID_COLS;
                    }
                }

                // L/R to change boxes
                if (kDown & HidNpadButton_L) {
                    int boxCount = static_cast<int>(trainer.getBoxCount());
                    selectedBoxIndex = (selectedBoxIndex - 1 + boxCount) % boxCount;
                }
                if (kDown & HidNpadButton_R) {
                    int boxCount = static_cast<int>(trainer.getBoxCount());
                    selectedBoxIndex = (selectedBoxIndex + 1) % boxCount;
                }

                // Touch: tap the ‹/› arrows to change box; tap a slot to select it, tap the
                // already-selected slot again to open its details (maps to A). Arrow/slot rects
                // are captured during the box draw (ids 1000/1001 for arrows, 0..slots-1 for cells).
                {
                    int tapped = touchedButtonId(touch);
                    int boxCount = static_cast<int>(trainer.getBoxCount());
                    if (tapped == 1000) {
                        selectedBoxIndex = (selectedBoxIndex - 1 + boxCount) % boxCount;
                    } else if (tapped == 1001) {
                        selectedBoxIndex = (selectedBoxIndex + 1) % boxCount;
                    } else if (tapped == 2000) {
                        kDown |= HidNpadButton_A;  // tapped the summary side-panel -> edit selected mon
                    } else if (tapped == 1002) {   // tapped the box-name pill -> focus header, rename next frame
                        if (trainer.supportsBoxNames() && !swapActive) {
                            selectedItemIndex = -1;         // highlight this frame; open the rename next frame
                            pendingHeaderRename = true;
                        }
                    } else if (tapped >= 0 && tapped < static_cast<int>(trainer.getSlotsPerBox())) {
                        if (selectedItemIndex == tapped) kDown |= HidNpadButton_A;  // second tap -> details
                        else selectedItemIndex = tapped;
                    }
                }

                // A button: box-name header -> rename; occupied slot -> details; empty slot -> create.
                if (kDown & HidNpadButton_A) {
                    if (selectedItemIndex == -1) {
                        renameBox(selectedBoxIndex);
                    } else if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                        selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                        const auto& pokemon = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                        if (pokemon && pokemon->speciesID() != 0) {   // skip empty / species-0 ghost slots
                            details.active = true;
                            details.leftScroll = 0;
                            details.source = EditSource::Box;
                            details.category = 0;
                            details.selectedStat = 0;
                            details.selectedField = 0;
                            details.hexMode = 0;
                            details.editing = false;
                            snapshotEditTarget();   // dirty-check baseline for Save/Discard on close
                        } else if (!storageSlotLocked(0, selectedBoxIndex, selectedItemIndex) && !swapActive) {
                            // Empty slot -> create a new Pokemon here, the same flow as the Storage view:
                            // pick a species, edit it in the details modal, Keep/Discard on exit. The
                            // creator's species-pick handler drops it into boxes[box][slot] via
                            // storageSlot(0,...) and opens the editor with EditSource::Box.
                            creator.active = true;
                            creator.pane = 0; creator.box = selectedBoxIndex; creator.slot = selectedItemIndex;
                            pickerKind = Dialogs::PickerKind::Species;
                            buildCreatorSpeciesOrder();   // only species this game can hold (unless illegal-values on)
                            pickerCount = static_cast<int>(pickerOrder.size());
                            pickerSel = 0;
                            pickerActive = true;
                        }
                    }
                }

                // X button: release the mon under the cursor, after a confirm. Blocked on a party-linked
                // slot (LGPE) -- releasing it would orphan the party pointer, the same rule the Storage
                // release menu enforces (D8). Reuses the global release-confirm flow (pane 0 = save box).
                // Skipped mid-swap -- while holding a grabbed mon, B cancels and Y drops.
                if ((kDown & HidNpadButton_X) && !swapActive) {
                    const bool inRange = selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                                         selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS);
                    if (inRange) {
                        const auto& pk = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                        if (pk && pk->speciesID() != 0) {
                            if (storageSlotLocked(0, selectedBoxIndex, selectedItemIndex)) {
                                postStatus("该宝可梦仍在同行队伍中，请先将它移出队伍。", 240);
                            } else {
                                releaseGroup = false;
                                releasePane = 0; releaseBox = selectedBoxIndex; releaseSlot = selectedItemIndex;
                                releaseConfirmActive = true;
                            }
                        }
                    }
                }

                // Y button: grab the slot under the cursor, then Y on another occupied slot
                // to swap them (Phase 3.1). Same-slot Y cancels the grab.
                if (kDown & HidNpadButton_Y) {
                    bool inRange = selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                                   selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS);
                    bool cursorOccupied = inRange && trainer.boxes[selectedBoxIndex][selectedItemIndex] != nullptr;

                    if (!swapActive) {
                        if (cursorOccupied) {
                            swapActive = true;
                            swapSourceBox = selectedBoxIndex;
                            swapSourceSlot = selectedItemIndex;
                        }
                    } else if (swapSourceBox == selectedBoxIndex && swapSourceSlot == selectedItemIndex) {
                        swapActive = false;  // grabbed same slot again -> cancel
                    } else {
                        // Occupied target -> swap; empty target -> move (swapping with an empty
                        // slot IS a move). LGPE storage is compacted on save so any gap left by a
                        // move is removed and the party/starter pointers are remapped.
                        (void)cursorOccupied;
                        trainer.swapBoxSlots(swapSourceBox, swapSourceSlot, selectedBoxIndex, selectedItemIndex);
                        hasUnsavedChanges = true;
                        swapActive = false;
                    }
                }
            }

            // Handle detail view navigation for Party mode
            if (selectedMode == ViewMode::Party) {
                constexpr int COLUMN_SIZE = 3;

                // Determine current column (0 = left, 1 = right)
                int currentColumn = (selectedPartyIndex >= COLUMN_SIZE) ? 1 : 0;
                int rowInColumn = selectedPartyIndex % COLUMN_SIZE;

                // Up/Down to navigate within current column
                if (kDown & HidNpadButton_Up) {
                    rowInColumn = (rowInColumn - 1 + COLUMN_SIZE) % COLUMN_SIZE;
                    selectedPartyIndex = currentColumn * COLUMN_SIZE + rowInColumn;
                }
                if (kDown & HidNpadButton_Down) {
                    rowInColumn = (rowInColumn + 1) % COLUMN_SIZE;
                    selectedPartyIndex = currentColumn * COLUMN_SIZE + rowInColumn;
                }

                // Left/Right to move between columns
                if (kDown & HidNpadButton_Left) {
                    if (currentColumn == 1) {
                        // Move from right column to left column, same row
                        selectedPartyIndex = rowInColumn;
                    }
                }
                if (kDown & HidNpadButton_Right) {
                    if (currentColumn == 0) {
                        // Move from left column to right column, same row
                        selectedPartyIndex = COLUMN_SIZE + rowInColumn;
                    }
                }

                // A button to view details
                if (kDown & HidNpadButton_A) {
                    // Only open if there's a pokemon in the selected slot
                    if (selectedPartyIndex >= 0 && selectedPartyIndex < static_cast<int>(trainer.party.size())) {
                        const Pokemon::Pokemon* pokemon = trainer.party[selectedPartyIndex].get();
                        if (pokemon && pokemon->speciesID() != 0) {  // Not empty
                            details.active = true;
                            details.leftScroll = 0;
                            details.source = EditSource::Party;
                            details.partyIndex = selectedPartyIndex;
                            details.category = 0;
                            details.selectedStat = 0;
                            details.selectedField = 0;
                            details.hexMode = 0;
                            details.editing = false;
                            snapshotEditTarget();   // dirty-check baseline for Save/Discard on close
                        }
                    }
                }
            }

            // Settings view: Up/Down select a row, A toggles it (0 = auto-backup, 1 = theme,
            // 2 = allow illegal values, 3 = Let's Go move warning, 4 = inject backups to game save).
            if (selectedMode == ViewMode::Settings) {
                constexpr int kSettingsRows = 5;
                if (kDown & HidNpadButton_Up)   settingsSelectedRow = (settingsSelectedRow - 1 + kSettingsRows) % kSettingsRows;
                if (kDown & HidNpadButton_Down) settingsSelectedRow = (settingsSelectedRow + 1) % kSettingsRows;
                int st = touchedButtonId(touch);
                if (st >= 0 && st < kSettingsRows) { settingsSelectedRow = st; kDown |= HidNpadButton_A; }
                if (kDown & HidNpadButton_A) {
                    if (settingsSelectedRow == 0)      g_autoBackupEnabled = !g_autoBackupEnabled;
                    else if (settingsSelectedRow == 1) applyTheme(g_themeMode == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark);
                    else if (settingsSelectedRow == 2) g_allowIllegalEdits = !g_allowIllegalEdits;
                    else if (settingsSelectedRow == 3) g_lgpeMoveWarn = !g_lgpeMoveWarn;
                    else {
                        // The master lock for writing into the real game save. Turning it ON only
                        // makes the "游戏存档" destination available in the save dialog -- it never
                        // makes a save destructive on its own, so no confirmation is needed here.
                        g_injectToGameSave = !g_injectToGameSave;
                        postStatus(g_injectToGameSave
                            ? "现在可以用旧备份覆盖游戏中的实时存档。"
                            : "备份将不能再覆盖游戏中的实时存档。", 300);
                    }
                    Utils::saveSettings();   // persist the change to settings.cfg
                }
            }

            return;  // Don't process other inputs while in detail view
        }

        // Normal mode navigation (not in detail view)
        if (kDown & HidNpadButton_B) {
            // Check for unsaved changes
            if (hasUnsavedChanges && !saveConfirmActive) {
                // Prompt to save changes before going back
                exitingWithUnsavedChanges = true;
                exitingViaPlus = false;  // Exiting via B button (go back)
                saveConfirmActive = true;
                return;
            }
            // No unsaved changes or already handled, go back immediately
            goBack = true;
        }

        // HOME main menu navigation. Pills (0 Pokemon, 1 Party, 2 Storage) are a vertical column;
        // the icons (3 Items, 4 Trainer, 5 Settings) are a horizontal row below. Up/Down move the
        // column and step into/out of the row; Left/Right move within the row (matching the layout).
        if (kDown & HidNpadButton_Up) {
            if (homeMenuIndex >= 3)      homeMenuIndex = 2;   // icon row -> Storage pill
            else if (homeMenuIndex > 0)  homeMenuIndex--;     // up the pill column
        }
        if (kDown & HidNpadButton_Down) {
            if (homeMenuIndex < 2)       homeMenuIndex++;     // down the pill column
            else if (homeMenuIndex == 2) homeMenuIndex = 3;   // Storage pill -> icon row
        }
        if (homeMenuIndex >= 3) {                             // horizontal icon row
            if (kDown & HidNpadButton_Left)  homeMenuIndex = (homeMenuIndex == 3) ? 5 : homeMenuIndex - 1;
            if (kDown & HidNpadButton_Right) homeMenuIndex = (homeMenuIndex == 5) ? 3 : homeMenuIndex + 1;
            kDown &= ~(HidNpadButton_Left | HidNpadButton_Right);  // consume L/R (don't also scroll the box preview)
        }
        switch (homeMenuIndex) {
            case 0: selectedMode = ViewMode::Boxes;    break;
            case 1: selectedMode = ViewMode::Party;    break;
            case 2: selectedMode = ViewMode::Storage;  break;
            case 3: selectedMode = ViewMode::Items;    break;
            case 4: selectedMode = ViewMode::Trainer;  break;
            case 5: selectedMode = ViewMode::Settings; break;
        }

        // Touch: tapping a menu pill/icon (ids 100-105) focuses it and acts like A.
        int homeTap = touchedButtonId(touch);
        if (homeTap >= 100 && homeTap <= 105) {
            homeMenuIndex = homeTap - 100;
            switch (homeMenuIndex) {
                case 0: selectedMode = ViewMode::Boxes;    break;
                case 1: selectedMode = ViewMode::Party;    break;
                case 2: selectedMode = ViewMode::Storage;  break;
                case 3: selectedMode = ViewMode::Items;    break;
                case 4: selectedMode = ViewMode::Trainer;  break;
                case 5: selectedMode = ViewMode::Settings; break;
            }
            kDown |= HidNpadButton_A;
        }

        // A / tap: activate the focused destination (enter the mode, or toggle theme for Settings).
        if (kDown & HidNpadButton_A) {
            switch (homeMenuIndex) {
                case 0:  // Pokemon (Boxes)
                    detailViewActive = true; selectedItemIndex = 0; currentPage = 0;
                    break;
                case 1:  // Party
                    detailViewActive = true; selectedPartyIndex = 0;
                    break;
                case 2:  // Storage (bank) — start on the save pane, Menu mode, nothing held/selected.
                    detailViewActive = true; storageFocusPane = 0; stSaveSlot = 0; stBankSlot = 0;
                    multiSel.clear(); storageMenuActive = false; groupMenuActive = false;
                    cursorMode = CursorMode::Menu;
                    break;
                case 3:  // Items
                    detailViewActive = true; selectedItemIndex = 0; currentPage = 0;
                    break;
                case 4:  // Trainer info
                    detailViewActive = true;
                    break;
                case 5:  // Settings screen (auto-backup + theme)
                    detailViewActive = true; settingsSelectedRow = 0;
                    break;
            }
        }

        // L/R to navigate categories (Items mode only, when not in detail view)
        if (selectedMode == ViewMode::Items) {
            if (kDown & HidNpadButton_L) {
                switch(trainer.getGameGroup()) {
                    case GameVersion::ZA: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_LZA) % POUCH_COUNT9_LZA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SV: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_SV) % POUCH_COUNT9_SV;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::PLA: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT8_LA) % POUCH_COUNT8_LA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::BDSP: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT8BDSP) % POUCH_COUNT8BDSP;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SWSH: {
                        selectedCategory = (selectedCategory - 1 + static_cast<int>(Trainer::PouchType8SWSH::Count)) % static_cast<int>(Trainer::PouchType8SWSH::Count);
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::GG: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT7_LGPE) % POUCH_COUNT7_LGPE;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::FRLG: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT3_FRLG) % POUCH_COUNT3_FRLG;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    default: break;
                }
            }
            if (kDown & HidNpadButton_R) {
                switch(trainer.getGameGroup()) {
                    case GameVersion::ZA: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_LZA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SV: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_SV;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::PLA: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT8_LA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::BDSP: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT8BDSP;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SWSH: {
                        selectedCategory = (selectedCategory + 1) % static_cast<int>(Trainer::PouchType8SWSH::Count);
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::GG: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT7_LGPE;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::FRLG: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT3_FRLG;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    default: break;
                }
            }
        }

        // L/R to navigate boxes (Boxes mode only, when not in detail view)
        if (selectedMode == ViewMode::Boxes) {
            if (kDown & HidNpadButton_L) {
                int boxCount = static_cast<int>(trainer.getBoxCount());
                selectedBoxIndex = (selectedBoxIndex - 1 + boxCount) % boxCount;
            }
            if (kDown & HidNpadButton_R) {
                int boxCount = static_cast<int>(trainer.getBoxCount());
                selectedBoxIndex = (selectedBoxIndex + 1) % boxCount;
            }
        }
    }

    void TrainerViewScreen::draw(PKSEFramebuffer& fb) {
        fb.clear(Colors::Background);

        // --- Title bar: the shared chrome, with game name + version + DLC as the subtitle ---
        std::string subtitle = titleName;
        if (!gameVersion.empty()) {
            subtitle += "  v" + gameVersion;
        }
        if (!trainer.saveRevisionString.empty() && trainer.saveRevisionString != "Base") {
            std::string revision = trainer.saveRevisionString;
            if (revision == "Crown Tundra") revision = "冠之雪原";
            else if (revision == "Isle of Armor") revision = "铠之孤岛";
            else if (revision == "Mega Dimension") revision = "超次元爆涌";
            else if (revision.starts_with("Rev ")) revision = "版本 " + revision.substr(4);
            subtitle += "  (" + revision + ")";
        }
        drawTitleBar(fb, subtitle);

        // Not entered -> the HOME main menu (replaces the old left trainer-info + mode-selector
        // chrome). Entered -> the selected mode's content spans the full width.
        const bool entered = detailViewActive;
        if (!entered) {
            Panels::drawHomeMenu(*this, fb);
        } else {
            const int contentX = LEFT_PANEL_X;
            const int contentPanelWidth = fb.getWidth() - contentX;
            switch (selectedMode) {
                case ViewMode::Party:
                    Panels::drawPartyPokemon(fb, trainer, contentX, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT, selectedPartyIndex);
                    break;
                case ViewMode::Boxes: {
                    // HOME layout: box on the left, the summary side-panel (render + hexagon) on the right.
                    constexpr int gap = 12, summaryW = 452;
                    const int boxW = contentPanelWidth - gap - summaryW;
                    Panels::drawBoxPokemon(*this, fb, contentX, CONTENT_PANEL_Y, boxW, CONTENT_PANEL_HEIGHT);
                    Panels::drawBoxSummaryPanel(*this, fb, contentX + boxW + gap, CONTENT_PANEL_Y, summaryW, CONTENT_PANEL_HEIGHT);
                    break;
                }
                case ViewMode::Items:
                    Panels::drawItems(*this, fb, contentX, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                    break;
                case ViewMode::Storage:
                    Panels::drawStorageView(*this, fb, contentX, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                    break;
                case ViewMode::Trainer:
                    drawTrainerView(*this, fb, contentX, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                    break;
                case ViewMode::Settings:
                    drawSettingsView(*this, fb, contentX, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                    break;
            }
        }

        // Draw instructions
        std::string instructions;
        if (swapActive) {
            instructions = "拿起中  |  方向键：移动光标  |  L/R：切换盒子  |  Y：放在此处  |  B：取消";
        } else if (statEdit.dialogActive) {
            if (statEdit.mode != Dialogs::StatEditMode::IV) {
                instructions = "上/下：个体值/觉醒值-努力值  |  方向键：调整数值  |  ZL/ZR：±100  |  A：确认  |  B：取消";
            } else {
                instructions = "上/下：个体值/觉醒值-努力值  |  方向键：调整数值  |  A：确认  |  B：取消";
            }
        } else if (itemEditDialogActive) {
            instructions = "左/右：±1  |  上/下：±10  |  ZL/ZR：±100  |  A：确认  |  B：取消";
        } else if (itemRemoveConfirmActive) {
            instructions = "A：移除  |  B：取消";
        } else if (saveConfirmActive) {
            instructions = "A：保存更改  |  B：取消";
        } else if (releaseConfirmActive) {
            instructions = "A：放生  |  B：取消";
        } else if (details.active) {
            instructions = "上/下：选择  |  A：编辑  |  Y：切换视图  |  B：关闭  |  X：保存";
        } else if (detailViewActive && selectedMode == ViewMode::Settings) {
            instructions = "上/下：选择  |  A：切换  |  B：返回";
        } else if (detailViewActive) {
            if (selectedMode == ViewMode::Items) {
                instructions = "上/下：选择  |  A：编辑数量  |  Y：添加  |  X：移除  |  左/右：翻页  |  L/R：分类  |  B：返回";
            } else if (selectedMode == ViewMode::Boxes) {
                if (details.active) {
                    if (details.category == 0) { // Main
                        instructions = details.editing
                            ? details.selectedField == 3
                                ? "上/下：选择字段  |  A：编辑  |  B：返回  |  X：保存  |  +：退出应用"
                                : "上/下：选择字段  |  B：返回  |  X：保存  |  +：退出应用"
                            : "上/下：选择分类  |  A：确定  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                    else if (details.category == 2) { // Stats
                        instructions = details.editing
                            ? "上/下：选择字段  |  A：编辑  |  B：返回  |  X：保存  |  +：退出应用"
                            : "上/下：选择分类  |  A：确定  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                    else {
                        instructions = "上/下：选择分类  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                }
                else {
                    // Only advertise Details / Grab-Move when the cursor is on an occupied slot.
                    bool occ = false;
                    if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                        selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                        const auto& bpk = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                        occ = bpk && bpk->speciesID() != 0;
                    }
                    // On the box-name header, A renames; on a slot, the usual actions. No "navigate up
                    // to rename" hint -- it's discoverable (the pill highlights) and just adds clutter.
                    if (selectedItemIndex == -1) {
                        instructions = "L/R：盒子  |  A：重命名  |  B：返回";
                    } else {
                        instructions = occ
                            ? "方向键：导航  |  L/R：盒子  |  A：详情  |  X：放生  |  Y：拿起／移动  |  B：返回"
                            : "方向键：导航  |  L/R：盒子  |  A：创建  |  B：返回";
                    }
                }
            } else if(selectedMode == ViewMode::Party) { // TODO: There HAS to be a better way of doing this without all of the if/else conditionals... probably will look into this at some point.
                if (details.active) {
                    if (details.category == 0) { // Main
                        instructions = details.editing
                            ? details.selectedField == 3
                                ? "上/下：选择字段  |  A：编辑  |  B：返回  |  X：保存  |  +：退出应用"
                                : "上/下：选择字段  |  B：返回  |  X：保存  |  +：退出应用"
                            : "上/下：选择分类  |  A：确定  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                    else if (details.category == 2) { // Stats
                        instructions = details.editing
                            ? "上/下：选择字段  |  A：编辑  |  B：返回  |  X：保存  |  +：退出应用"
                            : "上/下：选择分类  |  A：确定  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                    else {
                        instructions = "上/下：选择分类  |  B：关闭  |  X：保存  |  +：退出应用";
                    }
                }
                else {
                    instructions = "方向键：网格导航  |  A：查看详情  |  B：返回  |  +：退出应用";
                }
            } else if (selectedMode == ViewMode::Storage) {
                if (details.active) {
                    instructions = "上/下：分类  |  A：编辑  |  B：关闭";
                } else if (storageExitConfirmActive) {
                    instructions = "上/下：选择  |  A：确认  |  B：留在此处";
                } else if (storageMenuActive || groupMenuActive) {
                    instructions = "上/下：选择  |  A：确定  |  B：取消";
                } else if (releaseConfirmActive) {
                    instructions = "A：放生  |  B：取消";
                } else if (heldPokemon) {
                    instructions = "方向键：移动（边缘可跨区域）  |  L/R：盒子  |  A：放下／交换  |  B：返回";
                } else if (cursorMode == CursorMode::Menu) {
                    instructions = "方向键：移动  |  L/R：盒子  |  Y：模式（菜单）  |  A：菜单  |  X：排序  |  B：返回";
                } else if (cursorMode == CursorMode::Move) {
                    instructions = "方向键：移动  |  L/R：盒子  |  Y：模式（移动）  |  A：拿起  |  X：排序  |  B：返回";
                } else {
                    instructions = "方向键：移动  |  A：选择  |  -：选项  |  Y：模式（多选）  |  X：排序  |  B：清除";
                }
            } else if (selectedMode == ViewMode::Trainer) {
                instructions = "B：返回  |  +：退出应用";
            }
        } else {
            // HOME main menu.
            instructions = "方向键：导航  |  A：打开  |  B：返回  |  X：保存  |  +：退出应用";
        }
        // --- Nav bar: the contextual controls, drawn as controller badges ---
        drawNavBar(fb, instructions);
        const int footerY = fb.getHeight() - kNavBarH;

        // Transient storage status line (e.g. a refused cross-game drop), centered just above the footer.
        if (storageStatusFrames > 0 && !storageStatus.empty()) {
            int tw, th; fb.measureText(storageStatus, tw, th);
            const int padX = 18, bw = tw + padX * 2, bh = th + 14;
            const int bx = (fb.getWidth() - bw) / 2, by = footerY - bh - 12;
            fb.drawFilledRoundedRect(bx, by, bw, bh, 8, Colors::Panel);
            fb.drawRoundedRect(bx, by, bw, bh, 8, Colors::Accent, 2);
            fb.drawText(bx + padX, by + 7, storageStatus, Colors::Text);
        }

        // Draw dialogs on top of everything (Modals first, then dialogs)
        if (details.active) {
            Modals::drawPokemonDetailsModal(*this, fb);
        }
        if (pickerActive) {   // overlays the modal; registers its own touch buttons last
            Dialogs::drawPickerDialog(*this, fb);
        }
        if (itemEditDialogActive) {
            Dialogs::drawItemEditDialog(*this, fb);
        }
        if (itemRemoveConfirmActive) {
            Dialogs::drawItemRemoveConfirm(*this, fb);
        }
        if (statEdit.dialogActive) {
            Dialogs::drawStatEditDialog(*this, fb);
        }
        if (saveConfirmActive) {
            Dialogs::drawSaveConfirmDialog(*this, fb);
        }
        if (saveInjectConfirmActive) {   // overlays the picker
            Dialogs::drawSaveInjectConfirm(*this, fb);
        }
        if (storageMenuActive) {
            Panels::drawStorageActionMenu(*this, fb);
        }
        if (groupMenuActive) {
            Panels::drawStorageGroupMenu(*this, fb);
        }
        if (releaseConfirmActive) {
            Panels::drawStorageReleaseConfirm(*this, fb);
        }
        if (storageExitConfirmActive) {
            Panels::drawStorageExitConfirm(*this, fb);
        }
        if (creator.keepConfirmActive) {   // "保留这只新宝可梦吗？" overlays the creator's editor
            Panels::drawCreatorKeepConfirm(*this, fb);
        }
        if (lgpeMoveConfirmActive) {      // "传入或传出 Let's Go 会重置觉醒值／努力值" acknowledgement
            Panels::drawLgpeMoveConfirm(*this, fb);
        }
    }
}
