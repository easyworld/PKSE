/**
 * Convert.h - Cross-generation entity conversion for the bank (M5 Phase B)
 *
 * When a banked Pokemon is withdrawn into a save of a DIFFERENT game, its native bytes must be
 * re-formatted into the destination game's entity format. Following PKHeX/HOME: the origin identity
 * (Version, OT, ID, met/egg, HOME tracker, EC/PID/IVs/nature/gender/language/ribbons) is preserved
 * verbatim; only game-specific container fields are rewritten, and the checksum is refreshed.
 *
 * PKSE preserves origin identity + ability but SANITIZES the moveset for the destination: any move (or
 * relearn move) the destination species can't legally learn -- illegal for the species, OR not present in
 * that game at all -- is cleared to empty (see convert()). This is REQUIRED: Let's Go turns a mon carrying
 * an impossible move into a Bad Egg. The held item is sanitized the same way, against a per-game holdable-
 * item table: it is cleared if the destination cannot hold that id, which includes EVERY id for Let's Go and
 * Legends: Arceus (neither game has a held-item mechanic at all).
 * (PKHeX instead regenerates the whole moveset from the destination learnset via HOME; we keep the mon's
 * own legal moves and only drop the illegal ones.) Same-gen sibling pairs (SwSh<->BDSP, S/V<->Z-A) share
 * a near-identical layout (copy + null a couple of divergent fields). Cross-gen pairs (PK8<->PK9) are a
 * field remap: gender bit-position, Version/Language/FormArg/AffixedRibbon offsets, height/weight/scale,
 * HOME-tracker offset, and the record-flag / Tera / ObedienceLevel regions are re-laid-out; the Tera type
 * is imported from the species' primary type (PK8 has none). Legends: Arceus (PA8) has its whole Block
 * B/C/D at different offsets, so it is a full field relocation routed through the PK8 layout (PA8<->PK8,
 * then the PK8<->PK9 transforms for a Gen 9 endpoint); PA8-only data (GVs, Alpha, move-mastery) is dropped
 * and LA-only balls collapse to a Poke Ball. Let's Go (PB7, 260-byte Gen-7 record) likewise relocates
 * through the PK8 layout, but stat-training is NOT cross-converted: AVs/EVs are dropped so a mon entering
 * LGPE gets AVs=0 and one leaving gets EVs=0 (the UI asks the user to acknowledge this); its Level/stats/CP
 * tail is recomputed on convert. FireRed/LeafGreen (PK3, 80/100-byte GBA record) is the most divergent:
 * no EC (EC:=PID), nature/gender/shiny are all PID-derived, ability is a slot BIT resolved via the personal
 * table, species/items/text use Gen-3 id spaces, and IVs pack the ability bit at 31. Its remap converts
 * item ids (Item3to4), transcodes Gen-3<->Unicode names, and sanitizes the moveset against the real Gen-3
 * learnset like every other game; Level/stats are recomputed on convert. Going TO Gen 3, nature/gender become
 * PID-derived (Gen 3 stores neither) and custom nicknames collapse to the species name. Every conversion is
 * thus a PK8-layout hub: any of the seven games <-> any other (full two-way, beyond HOME's one-way Let's Go
 * limit), bounded only by destination dex-presence.
 *
 * Field-by-field offset map + rationale: docs/ROADMAP_TO_V1.md App. B.
 */
#ifndef CONVERSION_CONVERT_H
#define CONVERSION_CONVERT_H

#include <memory>

#include "Pokemon/Pokemon.h"
#include "Enums/GameVersion.h"

namespace Conversion {
    enum class Result {
        Ok,           // converted successfully
        SameGroup,    // no conversion needed -- the caller should move the original as-is
        Unsupported,  // not one of the seven supported mainline games (all of which now interconvert)
        NotInDex,     // species/form does not exist in the destination game's dex
        Blocked,      // destination refuses this species (e.g. BDSP Spinda / Nincada)
    };

    /// Converts `src` into `destGroup`'s entity format, preserving origin identity and refreshing the
    /// checksum. Returns a NEW entity, or nullptr with `result` set to the reason. A same-format pair
    /// yields SameGroup + nullptr (the caller should just move the original -- no conversion needed).
    std::unique_ptr<Pokemon::Pokemon> convert(const Pokemon::Pokemon& src, Enums::GameVersion destGroup, Result& result);

    /// True if `destGroup` can accept `src` (same group, or a supported+allowed conversion). Pure check
    /// (no allocation) for gating UI without performing the conversion.
    bool canConvert(const Pokemon::Pokemon& src, Enums::GameVersion destGroup, Result& result);

    /// Short human-facing reason for a non-Ok/SameGroup result, for on-screen feedback.
    const char* resultMessage(Result r);
}

#endif
