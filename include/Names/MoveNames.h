#ifndef NAMES_MOVE_NAMES_H
#define NAMES_MOVE_NAMES_H

#include <cstdint>

namespace Names {
    // Display name for a move ID (move 0 and out-of-range return "-").
    // Table is generated from PKHeX's move-name text by tools/gen_movenames.py.
    const char* getMoveName(uint16_t moveId);

    // Number of entries in the move-name table (for building a picker/list).
    unsigned getMoveCount();
}

#endif
