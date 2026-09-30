/**
 * GENERATED -- do not hand-edit. Generation 1 (Red/Blue/Yellow) data tables.
 *
 * Regenerate with `python tools/gen_gen1.py`. Source of truth is PKHeX.
 *
 * Gen 1 is not a special case of a modern Pokemon, it is a different format that happens to
 * share species names. It has ONE Special stat instead of SpA/SpD, a species index that is a
 * lookup rather than an offset, a type numbering that is neither modern nor contiguous, and its
 * own move PP. It has no abilities, natures, gender, egg groups, friendship, held items, balls,
 * met location, origin game, shininess or Pokerus -- those fields do not exist in the format and
 * must read as ABSENT, never as 0.
 *
 * Type ids in this table are already mapped to PKSE's TYPE_* constants (PokemonTypes.h). The raw
 * Gen 1 ROM numbering -- Bug 7, Ghost 8, Fire 20..Dragon 26, with 6 unused -- never escapes the
 * generator, so no call site can accidentally hand a ROM byte to the type-icon lookup.
 */
#ifndef POKEMON_GEN1TABLES_H
#define POKEMON_GEN1TABLES_H

#include <cstdint>

namespace Pokemon {

    /// Highest National Dex id Gen 1 knows.
    inline constexpr uint16_t MAX_SPECIES_GEN1 = 151;

    /// Number of moves Gen 1 defines (1..165; index 0 is the "no move" slot).
    inline constexpr uint16_t MAX_MOVE_GEN1 = 165;

    // GEN 1'S BASE STATS ARE NOT HERE. They live in PersonalInfo1RBY, with every other
    // group's, one table per save-format group -- including the gender ratio PKHeX substitutes
    // into byte 0x00 of a personal_rb row (Gen 1 has no gender; the value exists only so a
    // transferred Pokemon has one) and the catch rate, which is also the byte that becomes a
    // held item on a trade up to Gen 2. What stays here is what is Gen 1's ALONE and has no
    // counterpart in any other generation: the arbitrary internal species index, the ROM type
    // numbering, Gen 1's own move PP, and its own item id space.

    /**
     * Gen 1 internal species index -> National Dex id, and back.
     *
     * The two orderings are unrelated: Bulbasaur is internal 0x99, Ivysaur is 0x09. Both
     * directions return 0 for an id with no counterpart, which for `g1ToNational` means the
     * index is one of the ~39 unused slots (the "MissingNo." range) and the record is not a
     * real Pokemon.
     */
    uint16_t g1ToNational(uint8_t internalIndex) noexcept;
    uint8_t  nationalToG1(uint16_t species) noexcept;

    /**
     * Base PP for a Gen 1 move id (1..165), before PP Ups. 0 for an unknown id.
     *
     * Gen 1 has its own PP table; a modern lookup returns a different number for the same move.
     * PP Ups add `min(7, basePP / 5)` each, up to 3 -- the Gen 1/2 rule, which is not the modern
     * one either.
     */
    uint8_t getMovePPGen1(uint16_t move) noexcept;

    /// Applies `ppUpCount` (0-3) PP Ups to a Gen 1 move's base PP, per the Gen 1/2 rule.
    uint8_t getMovePPGen1WithUps(uint16_t move, uint8_t ppUpCount) noexcept;

    /**
     * Gen 1 ROM type id -> PKSE TYPE_* id. Returns 255 (TYPE_NONE) for an id Gen 1 never uses.
     *
     * The base-stat rows above are already converted, so this is only for type bytes read out
     * of a PK1 RECORD, which are stored raw. The two numberings are not the same and not even
     * close: Bug is 7 and Ghost 8 (6 is the unused "Bird" type), and the special types jump to
     * 20-26. Passing a raw Gen 1 type byte to anything expecting a modern id draws Bug as
     * Ghost, Ghost as Steel, and every special type off the end of the table.
     */
    uint8_t g1TypeToPKSE(uint8_t romType) noexcept;

    /**
     * PKSE TYPE_* id -> Gen 1 ROM type id. Returns 0 (Normal) for a type Gen 1 does not have.
     *
     * The inverse of g1TypeToPKSE, and needed because a PK1 STORES its types: the games copy
     * them out of the base-stat table when the Pokemon is created, so changing a record's
     * species has to rewrite them or the Pokemon keeps its old typing. Dark, Steel and Fairy
     * have no Gen 1 id at all -- they postdate the generation -- and collapse to Normal, which
     * is the only representable answer.
     */
    uint8_t pkseTypeToG1(uint8_t pkseType) noexcept;

    /// Highest Gen 1 item id. Ids are ONE BYTE and 0xFF terminates a bag, so 254 is the ceiling.
    inline constexpr uint8_t MAX_ITEM_GEN1 = 254;

    /**
     * Gen 1 item name by Gen 1 item id. "???" for an id the games never use.
     *
     * Gen 1's item ids are their own space and share nothing with the modern table -- the Gen 1
     * TMs are ordinary bag items at 201-250, which the modern numbering has nothing at. Naming a
     * Gen 1 bag through `Names::getItemName` gives a plausible, wrong answer for every slot, the
     * same way reading its species through the modern dex would.
     */
    const char* getItemNameGen1(uint8_t itemId) noexcept;

    /**
     * May this id legitimately sit in a Gen 1 bag?
     *
     * Narrower than "has a name", on purpose. 84-195 and 251-254 are unused slots with no name at
     * all; 7 and 44 are named (`????? (n)`) but unobtainable; 8 and 9 (Safari Ball, Pokedex) are
     * real named items that are not bag items. The item PICKER offers this set; the DISPLAY names
     * everything, because a save can hold a byte the picker would never offer and the user has to
     * be able to see what it is rather than a blank row.
     */
    bool isItemLegalGen1(uint8_t itemId) noexcept;

}

#endif  // POKEMON_GEN1TABLES_H
