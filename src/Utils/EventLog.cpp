#include "Utils/EventLog.h"

#include <cstdio>

#include "Enums/Ball.h"
#include "Enums/GameVersion.h"
#include "Enums/LanguageID.h"
#include "Names/FormNames.h"
#include "Names/ItemNames.h"
#include "Names/LocationNames.h"
#include "Names/MoveNames.h"
#include "Pokemon/Pokemon.h"
#include "Trainer/Trainer.h"
#include "Utils/StringHelpers.h"

namespace Utils {
    namespace {
        // Quote and escape. A nickname is user-controlled text that reaches this log verbatim, so a
        // mon called `He said "hi"` must not be able to split one field into three.
        std::string quoted(const std::string& value) {
            std::string out;
            out.reserve(value.size() + 2);
            out += '"';
            for (const char c : value) {
                if (c == '"' || c == '\\') out += '\\';
                out += c;
            }
            out += '"';
            return out;
        }

        std::string num(uint32_t v) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%u", v);
            return std::string(buf);
        }

        std::string hex32(uint32_t v) {
            char buf[16];
            snprintf(buf, sizeof(buf), "0x%08X", v);
            return std::string(buf);
        }

        // Empty rather than "(none)" so an absent name is absent from the line entirely -- a field
        // that is present only when it means something is easier to grep than one that is always
        // there and usually blank.
        std::string safeName(const char* s) {
            return (s != nullptr && s[0] != '\0') ? std::string(s) : std::string();
        }

    }  // namespace

    std::string itemName(uint16_t itemId, Enums::GameVersion gameGroup) {
        if (itemId == 0) return std::string();
        return (gameGroup == Enums::GameVersion::FRLG)
                   ? safeName(Names::getItemNameG3(itemId))
                   : safeName(Names::getItemName(itemId));
    }

    std::string logField(const char* key, const std::string& value) {
        return std::string(key) + "=" + quoted(value);
    }

    std::string logSlot(const char* key, int pane, int box, int slot) {
        // 1-based box/slot to match the UI. Pane is named, not numbered: "1" would be meaningless in
        // a log read six months later, and the save/bank distinction is the whole point of the field.
        return std::string(key) + "=" + (pane == 0 ? "save" : "bank")
             + "/box" + num(static_cast<uint32_t>(box + 1))
             + "/slot" + num(static_cast<uint32_t>(slot + 1));
    }

    MonOrigin classifyMonOrigin(const Pokemon::Pokemon& pk, const Trainer::Trainer& save) {
        const std::string ot = utf16ToUtf8(pk.otName());
        if (ot.empty()) return MonOrigin::NoOT;

        // All three must agree. Name alone is not enough (two trainers can share one), and ID alone
        // is not either -- a renamed trainer keeps their ID. PKHeX uses the same triple to decide
        // whether the handler slot should be filled, which is exactly the behaviour being diagnosed
        // when someone reports an OT/HT problem.
        const bool sameId     = (pk.id32() == save.ID32);
        const bool sameName   = (ot == save.trainerName);
        const bool sameGender = (pk.otGender() == save.trainerGender);
        return (sameId && sameName && sameGender) ? MonOrigin::OwnCaught : MonOrigin::Traded;
    }

    const char* monOriginName(MonOrigin origin) {
        switch (origin) {
            case MonOrigin::OwnCaught: return "OWN";
            case MonOrigin::Traded:    return "TRADED";
            case MonOrigin::NoOT:      return "NO_OT";
        }
        return "?";
    }

    std::string briefMon(const Pokemon::Pokemon& pk) {
        const std::string species = Names::getDisplayName(pk.speciesID(), pk.form(),
                                                          Trainer::getSpeciesName(pk.speciesID()));
        std::string out = logField("species", species)
                        + " form=" + num(pk.formID())
                        + " lv=" + num(pk.level());

        const std::string nick = utf16ToUtf8(pk.nickname());
        if (!nick.empty() && nick != species) out += " " + logField("nick", nick);
        if (pk.isShiny(pk.id32(), pk.species())) out += " shiny=Y";
        if (pk.isEgg()) out += " egg=Y";
        return out;
    }

    std::string describeMon(const Pokemon::Pokemon& pk, const Trainer::Trainer& save) {
        std::string out = briefMon(pk);

        out += std::string(" gender=") + pk.genderSymbol();
        out += " fmt=" + Enums::getGameVersionName(pk.getGameGroup());

        // ---- Ownership. The reason most of this function exists. ----
        const MonOrigin origin = classifyMonOrigin(pk, save);
        out += std::string(" own=") + monOriginName(origin);

        const std::string ot = utf16ToUtf8(pk.otName());
        if (!ot.empty()) {
            out += " " + logField("ot", ot);
            out += std::string(" otgender=") + (pk.otGender() == 0 ? "M" : "F");
            // Both spellings of the ID. The six-digit form is what the game and the user see; the
            // raw id32 is what the code compares, and a report where those disagree is the bug.
            const uint8_t ver = pk.originGame();
            const bool sixDigit = (ver != 0) ? Enums::usesSixDigitTrainerID(ver)
                                             : (pk.getGameGroup() != Enums::GameVersion::FRLG);
            out += " otid=" + num(sixDigit ? (pk.id32() % 1000000u) : (pk.id32() & 0xFFFFu));
            out += " otid32=" + hex32(pk.id32());
        }
        // Present only once a mon has actually been handled by someone else, matching the details
        // view. An empty HT on a traded mon is itself a finding, hence logging the handler flag
        // whenever either side of the pair exists.
        const std::string ht = utf16ToUtf8(pk.htName());
        if (!ht.empty()) {
            out += " " + logField("ht", ht);
            out += std::string(" htgender=") + (pk.htGender() == 0 ? "M" : "F");
            out += std::string(" handler=") + (pk.currentHandler() == 0 ? "OT" : "HT");
        }
        // The save's own trainer, so a log read in isolation can verify the OWN/TRADED call above
        // instead of asking the reporter who they are.
        out += " " + logField("saveot", save.trainerName) + " saveid32=" + hex32(save.ID32);

        // ---- Origin / met ----
        { const std::string og = Enums::getOriginGameName(pk.originGame());
          if (!og.empty()) out += " " + logField("origin", og); }
        const uint8_t fmtVer = Enums::getGroupRepVersion(pk.getGameGroup());
        { const std::string met = safeName(
              Names::getMetLocationName(Enums::locationTableVersion(pk.originGame(), fmtVer, false),
                                        pk.metLocation()));
          if (!met.empty()) out += " " + logField("met", met); }
        out += " metloc=" + num(pk.metLocation()) + " metlv=" + num(pk.metLevel());
        if (pk.metMonth() != 0 && pk.metDay() != 0) {
            char date[16];
            snprintf(date, sizeof(date), "%04u-%02u-%02u", 2000 + pk.metYear(), pk.metMonth(), pk.metDay());
            out += std::string(" metdate=") + date;
        }
        if (pk.eggLocation() != 0) {
            out += " eggloc=" + num(pk.eggLocation());
            if (pk.isFatefulEncounter()) out += " fateful=Y";
        } else if (pk.isFatefulEncounter()) {
            out += " fateful=Y";
        }

        // ---- Everything else worth having ----
        { const std::string ball = safeName(Enums::getBallName(pk.ball()));
          if (!ball.empty()) out += " " + logField("ball", ball); }
        { const std::string lang = safeName(Enums::getLanguageName(pk.language()));
          if (!lang.empty()) out += " " + logField("lang", lang); }
        { const std::string ability = safeName(Trainer::getAbilityName(pk.ability()));
          if (!ability.empty()) out += " " + logField("ability", ability); }
        out += " abilityid=" + num(pk.ability());
        { const std::string nature = safeName(Trainer::getNatureName(pk.nature()));
          if (!nature.empty()) out += " " + logField("nature", nature); }
        { const std::string held = itemName(pk.heldItem(), pk.getGameGroup());
          if (!held.empty()) out += " " + logField("held", held); }
        out += " friendship=" + num(pk.friendship());
        out += " pid=" + hex32(pk.pid()) + " ec=" + hex32(pk.encryptionConstant());
        out += " exp=" + num(pk.exp());

        out += " ivs=" + num(pk.ivHP()) + "/" + num(pk.ivATK()) + "/" + num(pk.ivDEF())
             + "/" + num(pk.ivSPA()) + "/" + num(pk.ivSPD()) + "/" + num(pk.ivSPE());
        out += " evs=" + num(pk.evHP()) + "/" + num(pk.evATK()) + "/" + num(pk.evDEF())
             + "/" + num(pk.evSPA()) + "/" + num(pk.evSPD()) + "/" + num(pk.evSPE());
        if (pk.hasAwakeningValues()) {
            out += " avs=" + num(pk.avHP()) + "/" + num(pk.avATK()) + "/" + num(pk.avDEF())
                 + "/" + num(pk.avSPA()) + "/" + num(pk.avSPD()) + "/" + num(pk.avSPE());
        }

        // Moves are logged by NAME as well as id: a move that is legal in one game and dummied in
        // another is a known way to produce a Bad Egg, and chasing that from bare numbers is slow.
        std::string moves;
        for (int i = 0; i < 4; ++i) {
            const uint16_t id = pk.move(i);
            if (id == 0) continue;
            if (!moves.empty()) moves += ",";
            const std::string name = safeName(Names::getMoveName(id));
            moves += name.empty() ? ("#" + num(id)) : name;
        }
        if (!moves.empty()) out += " " + logField("moves", moves);

        return out;
    }
}  // namespace Utils
