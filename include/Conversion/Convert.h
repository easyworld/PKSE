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
    ///
    /// `destOriginVersion` is the exact destination GAME's origin byte, used only where an origin cannot
    /// be carried across and has to be restamped -- today that is the Gen 3 down-convert, whose origin
    /// field is 4 bits wide and cannot hold a modern game's id. It must be passed because a game *group*
    /// deliberately collapses a version pair into one value: `FRLG` cannot say FireRed from LeafGreen,
    /// and stamping a fixed member of the pair is wrong half the time. 0 = unknown, which falls back to
    /// the group's representative version (the old, always-FireRed behaviour).
    std::unique_ptr<Pokemon::Pokemon> convert(const Pokemon::Pokemon& src, Enums::GameVersion destGroup, Result& result,
                                              uint8_t destOriginVersion = 0);

    /// True if `destGroup` can accept `src` (same group, or a supported+allowed conversion). Pure check
    /// (no allocation) for gating UI without performing the conversion.
    bool canConvert(const Pokemon::Pokemon& src, Enums::GameVersion destGroup, Result& result);

    /// Short human-facing reason for a non-Ok/SameGroup result, for on-screen feedback.
    const char* resultMessage(Result r);

    /// Byte offset of **AffixedRibbon** in a game's entity format, or 0 for the formats that have no
    /// such field (Gen 3's PK3 and Let's Go's PB7).
    ///
    /// AffixedRibbon names the single ribbon the game DISPLAYS on the summary screen. Its "none" value
    /// is 0xFF, **not** 0 -- 0 is a real index, and index 0 is the Kalos Champion ribbon. So any code
    /// path that leaves this byte at its zero-initialised default hands the mon a Kalos Champion ribbon
    /// it does not own. That has now bitten twice, from two different directions (the creator, then
    /// cross-gen conversion), so the offset lives here once and both call sites read it.
    size_t affixedRibbonOffset(Enums::GameVersion group) noexcept;

    /// The value meaning "display no ribbon". Not zero -- see affixedRibbonOffset().
    inline constexpr uint8_t AFFIXED_RIBBON_NONE = 0xFF;

    /// Repairs an invalid AffixedRibbon on `pk` **in place**, re-checksumming if it changed. Returns
    /// true when it changed something.
    ///
    /// Invalid means the byte reads 0 -- "display ribbon index 0", the Kalos Champion ribbon -- while
    /// the mon does not own that ribbon. Verified against real saves: every genuinely game-caught mon
    /// carries 0xFF, including ones that DO own ribbons, so a 0 here is never something the game wrote.
    /// The owned-ribbon guard means a deliberately affixed ribbon is never disturbed.
    ///
    /// Call on the way INTO a save (conversion, and the same-group path that skips conversion). Not on
    /// deposit: the bank stores native bytes untouched by design.
    bool normalizeAffixedRibbon(Pokemon::Pokemon& pk);
}

#endif
