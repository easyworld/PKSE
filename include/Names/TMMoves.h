/**
 * TMMoves.h - TM/HM item -> taught move lookup (per generation)
 *
 * Maps a TM/HM *item id* to the *move id* that machine teaches. The same item
 * id teaches a DIFFERENT move in each generation (every game reshuffles its TM
 * list), so the lookup is keyed off the game GROUP (see Enums::GameVersion:
 * GG / SWSH / BDSP / SV / PLA / ZA).
 *
 * Data is extracted from PKHeX. Two pieces are combined per game:
 *   - the per-item TM ordering from the ItemStorage<gen> "Machine" tables, and
 *   - the machine-index -> move-id mapping from PersonalInfo<gen>.MachineMoves
 *     (SWSH: MachineMovesTechnical + MachineMovesRecord) /
 *     LearnSource<gen>.MachineMoves.
 *
 * The caller resolves the returned move id to a display name via
 * Names::getMoveName.
 */

#ifndef NAMES_TM_MOVES_H
#define NAMES_TM_MOVES_H

#include <cstdint>

#include "Enums/GameVersion.h"

namespace Names {
    /**
     * Gets the move a TM/HM item teaches in a given game group.
     *
     * @param group  Game group (Enums::GameVersion), e.g. GG, SWSH, BDSP, SV,
     *               PLA, ZA. The individual game ids (SW/SH, BD/SP, SL/VL, ...)
     *               are also accepted and treated as their group.
     * @param itemId The bag item id of the TM/HM (e.g. 328 = classic "TM01").
     * @return The move id taught by that machine, or 0 if the item is not a
     *         TM/HM/TR in that game (or the group has no TM item table, such as
     *         Legends: Arceus, which teaches moves without TM items).
     */
    uint16_t getTMMove(Enums::GameVersion group, uint16_t itemId);
}

#endif
