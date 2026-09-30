/**
 * GENERATED -- do not hand-edit. Generation 2 (Gold/Silver/Crystal) item data.
 *
 * Regenerate with `python tools/gen_personaltables.py`. Source of truth is PKHeX.
 *
 * The sibling of Gen1Tables.h, and separate for the same reason: Gen 2's item ids are its own
 * space, sharing nothing with the modern table. Its TMs are ordinary bag items at 191-249, where
 * the modern numbering has nothing at all, so naming a Gen 2 bag through Names::getItemName is
 * wrong in every slot rather than blank -- and a plausible wrong answer is the failure that hides.
 *
 * Gen 2's PER-SPECIES data is not here. It lives in PersonalInfo2GSC.h, one file per
 * save-format group like every other generation's.
 */
#ifndef POKEMON_GEN2TABLES_H
#define POKEMON_GEN2TABLES_H

#include <cstdint>

namespace Pokemon
{
    /// Highest Gen 2 item id. Ids are ONE BYTE and 0xFF terminates a pouch.
    inline constexpr uint8_t MAX_ITEM_GEN2 = 255;

    /// Gen 2 item name by Gen 2 item id. "???" for an id the games never use.
    const char *getItemNameGen2(uint8_t itemId) noexcept;

    /// May this id legitimately sit in a Gen 2 bag? Narrower than "has a name": the unused
    /// slots are named `???` and the picker must not offer them. The DISPLAY names everything,
    /// because a save can hold a byte the picker would never offer and the user has to be able
    /// to see what it is rather than a blank row.
    bool isItemLegalGen2(uint8_t itemId) noexcept;
}

#endif  // POKEMON_GEN2TABLES_H
