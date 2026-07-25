#ifndef NAMES_ITEMNAMES_H
#define NAMES_ITEMNAMES_H

#include <cstdint>
#include <cstddef>

namespace Names {
    /// Item name by modern (Gen 4+) item id. Returns "???" if out of range.
    const char* getItemName(uint16_t itemId);
    /// Number of entries in the modern item-name table.
    size_t getItemCount();

    // ---- Gen 3 (GBA) item id <-> modern item id (PKHeX ItemConverter.Item3to4) ----
    /// Convert a Gen 3 item id to the modern (Gen 4+) id; 0 if it has no modern equivalent.
    uint16_t itemG3ToModern(uint16_t g3Id);
    /// Convert a modern item id back to its Gen 3 id; 0 if it doesn't exist in Gen 3 (drop on transfer).
    uint16_t itemModernToG3(uint16_t modernId);
    /// Item name for a Gen 3 item id (converts to the modern id and reuses the name table; HMs named directly).
    const char* getItemNameG3(uint16_t g3Id);
    /// Number of entries in the Gen 3 item-name table (Gen 3 ids are a SEPARATE, much smaller space).
    size_t getItemCountG3();
}

#endif  // NAMES_ITEMNAMES_H
