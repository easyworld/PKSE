/**
 * Maps a met/egg-location ID to its display name. The same numeric location ID
 * refers to a DIFFERENT place in each generation, so the lookup is keyed off
 * the pokemon's ORIGIN game version (the standard version id stored in the PKM,
 * see Enums::GameVersion).
 *
 * IDs are BANKED (id / 10000): bank 0 = in-world locations, bank 3 (30000) =
 * Link Trade / region + HOME/GO transfers, bank 4 (40000) = events,
 * bank 6 (60000) = egg received-from sources. A traded/transferred/egg pokemon
 * therefore has a high-banked id; resolving only bank 0 renders it "(none)".
 * Tables are extracted from PKHeX's met-location text resources and the lookup
 * mirrors PKHeX's LocationSet6.GetLocationName.
 */

#ifndef NAMES_LOCATION_NAMES_H
#define NAMES_LOCATION_NAMES_H

#include <cstdint>
#include <cstddef>

namespace Names
{
    /// locationId routes to its bank (id / 10000) and is indexed at id minus the bank base.
    /// "" when the id is out of range or the origin version has no table.
    const char *getMetLocationName(uint8_t originVersion, uint16_t locationId);

    /**
     * The raw bank-0 (in-world) name table for an origin game, for ENUMERATING a
     * picker (the id->name lookup above answers a single id). The special banks
     * 3/4/6 are trade/transfer/egg markers, not user-assignable met locations, so
     * they're deliberately excluded here. names[id] is the location name, possibly
     * "" for gaps / PKHeX's "no location" sentinel -- callers filter those out.
     * Returns {nullptr, 0} when the origin version has no supported table.
     */
    struct LocationTable
    {
        const char *const *names;
        size_t count;
    };
    LocationTable getLocationTable(uint8_t originVersion);

    /// Ids in the table below start here: entry i is location EGG_SOURCE_LOCATION_BASE + i.
    inline constexpr uint16_t EGG_SOURCE_LOCATION_BASE = 60000;

    /**
     * The bank-6 (egg received-from) table for an origin game, for enumerating the EGG LOCATION
     * picker.
     *
     * These are the values a hatched egg's location field actually holds -- "a Nursery worker",
     * "Nursery Couple", "Riley" -- so the egg picker must offer them, while the met picker
     * legitimately does not (they are not places a Pokemon is met). getMetLocationName() has always
     * been able to DISPLAY them; the report was that nothing could select one.
     *
     * Returns {nullptr, 0} for the origins with no such bank (Gen 3, Gen 4 -- which banks its ids
     * differently -- and Let's Go), and the caller then offers only the in-world list.
     */
    LocationTable getEggSourceLocationTable(uint8_t originVersion);
}

#endif
