/**
 * Type IDs match the MoveType enum. There are NINETEEN names and only EIGHTEEN types a species
 * can be, because index 18 is Stellar -- a TERA type, which no species and no move has. The two
 * counts below answer different questions and must not be merged; see each one.
 */

#ifndef NAMES_TYPE_NAMES_H
#define NAMES_TYPE_NAMES_H

#include <cstdint>

namespace Names
{
    const char *getTypeName(uint8_t typeId);

    constexpr uint8_t getTypeCount() { return 18; }

    constexpr uint8_t getTypeNameCount() { return 19; }
}

#endif
