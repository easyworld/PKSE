#ifndef TRAINER_INVENTORY8LA_H
#define TRAINER_INVENTORY8LA_H

#include <cstdint>
#include <cstddef>

namespace Trainer {
    // ========================================
    // Legends: Arceus item pouches
    // ========================================
    // Unlike the *indexed* Gen 9 pouches (an item's count stored at itemId * 0x10), Legends: Arceus
    // stores each pouch as a PACKED LIST of 4-byte entries: { itemId (u16 @0x00), count (u16 @0x02) }.
    // So there is no valid-item-ID table — the item id rides with each entry. The four pouches are
    // four separate save blocks (keys ITEM_REGULAR8_LA / ITEM_KEY8_LA / ITEM_STORED8_LA /
    // ITEM_RECIPE8_LA, defined in Trainer8LA.h). An entry with itemId == 0 is an empty slot.

    enum class PouchType8LA : uint32_t {
        Regular  = 0,   // General items (medicine, balls, berries, evolution items, ...)
        KeyItems = 1,   // Key items
        Stored   = 2,   // Item storage box (overflow from the satchel)
        Recipes  = 3,   // Crafting recipes
    };

    constexpr size_t POUCH_COUNT8_LA     = 4;
    constexpr size_t ITEM_ENTRY_SIZE8_LA = 4;   // { itemId u16, count u16 }

    // All four inventories are shown and editable. The first two are the in-game satchel's tabs
    // ("Everyday Items" = the General consumable pool, and "Key Items"); the other two are separate
    // in-game menus -- the base item-storage box ("Stored Items", the satchel's overflow) and
    // crafting "Recipes" -- named accurately so they don't read as duplicates of the satchel.
    struct PouchInfo8LA {
        const char* name;
    };

    inline PouchInfo8LA getPouchInfo8LA(PouchType8LA pouch) {
        switch (pouch) {
            case PouchType8LA::Regular:  return {"日常道具"};
            case PouchType8LA::KeyItems: return {"重要道具"};
            case PouchType8LA::Stored:   return {"收纳道具"};   // base storage-box overflow
            case PouchType8LA::Recipes:  return {"工艺配方"};    // crafting recipes
        }
        return {"日常道具"};
    }
}

#endif
