/**
 * Maps a TM/HM *item id* to the *move id* that machine teaches. The same item
 * id teaches a DIFFERENT move in each generation (every game reshuffles its TM
 * list), so the lookup is keyed off the game GROUP (see Enums::GameVersion:
 * GG / SWSH / BDSP / SV / PLA / ZA).
 *
 * Data is extracted from PKHeX. Two pieces are combined per game:
 *   - the per-item TM ordering from the ItemStorage<generation> "Machine" tables, and
 *   - the machine-index -> move-id mapping from PersonalInfo<generation>.MachineMoves
 *     (SWSH: MachineMovesTechnical + MachineMovesRecord) /
 *     LearnSource<generation>.MachineMoves.
 *
 * The caller resolves the returned move id to a display name via
 * Names::getMoveName.
 */

#ifndef NAMES_TM_MOVES_H
#define NAMES_TM_MOVES_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Names
{
    /// 0 when the item is not a TM/HM/TR in that game, or the group teaches moves without TM
    /// items at all (Legends: Arceus). An individual game id is accepted as its group.
    uint16_t getTMMove(Enums::GameVersion group, uint16_t itemId);
}

#endif
