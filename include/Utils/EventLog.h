#ifndef UTILS_EVENTLOG_H
#define UTILS_EVENTLOG_H

#include <cstdint>
#include <string>

#include "Enums/GameVersion.h"

namespace Pokemon { class Pokemon; }
namespace Trainer { class Trainer; }

/**
 * Formatting for the `[EVENT]` action log (the sink itself is Utils::logEventToFile in Logger.h).
 *
 * This exists so every call site spells a Pokemon the SAME way. The events are read by someone who
 * was not there, comparing a user's log against what they said they did, so a field that is named
 * `ot` in one line and `otName` in another -- or present in one and missing in another -- costs
 * exactly the time the log was supposed to save. Build lines from these helpers, not by hand.
 *
 * GUARD THE CALL SITES with `if (g_debugLogging)`. describeMon does dozens of table lookups and
 * string joins, and C++ evaluates arguments before the callee gets to check anything -- so an
 * unguarded call pays the full cost on every drop, release and edit even with logging switched off,
 * which for a multi-select is that cost times thirty. The sink is still gated internally, so an
 * unguarded call is a performance bug rather than a correctness one, but on a handheld running a
 * multi-select drop it is a bug worth not writing.
 */
namespace Utils {
    /// How a Pokemon relates to the trainer whose save it currently sits in.
    ///
    /// This is the single most useful fact in a bug report about a Pokemon, because the two cases
    /// travel down different code paths: a traded mon carries a foreign OT that the handler/HT
    /// re-stamp has to preserve, and a mon caught by the current trainer does not. "It broke" plus
    /// "it was traded" is frequently the entire diagnosis.
    enum class MonOrigin {
        OwnCaught,   ///< OT name, full 32-bit trainer ID, and OT gender all match this save's trainer
        Traded,      ///< any of those three differ -- it came from somebody else
        NoOT         ///< no OT recorded at all (Gen 3 blanks, or a genuinely aberrant record)
    };

    /// Classify against the save's own trainer. Compares the FULL id32 (TID+SID), not the visible
    /// TID: two trainers sharing a visible ID is ordinary, and treating those as the same trainer
    /// would silently mislabel a traded mon as home-caught.
    MonOrigin classifyMonOrigin(const Pokemon::Pokemon& pk, const Trainer::Trainer& save);

    /// "OWN" / "TRADED" / "NO_OT" -- the token written into the log.
    const char* monOriginName(MonOrigin origin);

    /// Everything worth knowing about one Pokemon, as `KEY=value` pairs on a single line: identity,
    /// ownership and OT/HT, origin and met data, ball/language/ability/nature, PID/EC, IVs/EVs (AVs
    /// too where the game has them), and moves. Long by design -- this is the line that answers
    /// "what exactly was the user holding when it went wrong?" without a follow-up question.
    ///
    /// `save` supplies only the ownership comparison; the mon's own fields supply everything else.
    std::string describeMon(const Pokemon::Pokemon& pk, const Trainer::Trainer& save);

    /// Species/form/nickname/level/shiny only -- for events that name a mon in passing (a pickup, a
    /// slot in a multi-select) where a full dump per Pokemon would bury the action itself.
    std::string briefMon(const Pokemon::Pokemon& pk);

    /// Name an item id in the id space of the game holding it. Gen 3 is the trap: the same number
    /// names a DIFFERENT item there, so a single shared table would produce a log that confidently
    /// states the wrong item -- worse than one that says nothing. Empty for id 0 (no item).
    std::string itemName(uint16_t itemId, Enums::GameVersion gameGroup);

    /// `KEY="value"` with embedded quotes and backslashes escaped, so a nickname containing a quote
    /// cannot break the line's field structure. Nicknames are user-controlled text; they will.
    std::string logField(const char* key, const std::string& value);

    /// A storage location as `pane/box/slot`, 1-based for boxes and slots to match what the UI shows
    /// the user. A log that counts from 0 while the screen counts from 1 is a reliable way to chase
    /// the wrong slot.
    std::string logSlot(const char* key, int pane, int box, int slot);
}

#endif
