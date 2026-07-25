/**
 * LocationNames.h - Met-location Name Lookup (per generation)
 *
 * Maps a met-location ID to its display name. The same numeric location ID
 * refers to a DIFFERENT place in each generation, so the lookup is keyed off
 * the mon's ORIGIN game version (the standard version id stored in the PKM,
 * see Enums::GameVersion).
 *
 * Tables are extracted from PKHeX's met-location text resources and cover the
 * MAIN (0x0-based) location list for each game group.
 */

#ifndef NAMES_LOCATION_NAMES_H
#define NAMES_LOCATION_NAMES_H

#include <cstdint>
#include <cstddef>

namespace Names {
    /**
     * Gets the met-location name for a mon of a given origin game.
     *
     * @param originVersion Standard game-version id (Enums::GameVersion), e.g.
     *                      42/43 = Let's Go Pikachu/Eevee, 44/45 = Sword/Shield,
     *                      47 = Legends Arceus, 48/49 = Brilliant Diamond/Shining
     *                      Pearl, 50/51 = Scarlet/Violet, 52 = Legends Z-A.
     * @param locationId    The met-location ID (indexes the origin's main table).
     * @return Location name string, or "" if the id is unknown/out of range or
     *         the origin version has no supported table.
     */
    const char* getMetLocationName(uint8_t originVersion, uint16_t locationId);

    /**
     * The raw met-location name table for an origin game, for ENUMERATING a picker
     * (the id->name lookup above answers a single id). names[id] is the location name,
     * possibly "" for gaps / PKHeX's "no location" sentinel -- callers filter those out.
     * Returns {nullptr, 0} when the origin version has no supported table.
     */
    struct LocationTable { const char* const* names; size_t count; };
    LocationTable getLocationTable(uint8_t originVersion);
}

#endif
