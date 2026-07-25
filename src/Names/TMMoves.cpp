/**
 * TMMoves.cpp - TM/HM item -> taught move lookup (per generation)
 *
 * Extracted from PKHeX. Each table below lists the move id taught at a given
 * "machine index"; how a bag item id maps onto that index differs per game and
 * is handled in getTMMove():
 *
 *   Group  Move table (PKHeX source)                        Item ranges -> index
 *   -----  -----------------------------------------------  --------------------
 *   FRLG   PersonalInfo3.MachineMovesTechnical (50)         289-338 -> i-289
 *          + PersonalInfo3.MachineMovesHidden (8, HMs)      339-346 -> i-339
 *   GG     LearnSource7GG.MachineMoves (60)                 328-387 -> i-328
 *   SWSH   PersonalInfo8SWSH.MachineMovesTechnical (100)    TM00 at index 0;
 *          + PersonalInfo8SWSH.MachineMovesRecord (100, TR) items 328.. -> 1..
 *   BDSP   PersonalInfo8BDSP.MachineMoves (100)             328-419 -> i-328,
 *                                                           420-427 -> 92+(i-420)
 *   SV     PersonalInfo9SV.MachineMoves (230)               TM000 at index 0;
 *                                                           items 328.. -> 1..
 *   ZA     PersonalInfo9ZA.MachineMoves (160)               parallel to
 *                                                           ItemStorage9ZA.TM
 *   PLA    (no TM items - Legends: Arceus teaches moves differently) -> 0
 *
 * The classic item ids 328-419 have been "TM01".."TM92" since Gen 4. SWSH and
 * SV additionally expose item 1230 as "TM00"/"TM000" (Mega Punch), which sits
 * at move index 0 while item 328 ("TM01"/"TM001") starts at index 1 - hence the
 * +1 offset for those two groups but not for GG/BDSP/ZA (which have no TM00).
 */

#include "Names/TMMoves.h"

#include <cstddef>

namespace Names {

    using Enums::GameVersion;

    // Let's Go, Pikachu!/Eevee!  (PKHeX LearnSource7GG.MachineMoves)
    // TM01..TM60 == items 328..387; index i == item (328 + i). No TM00, no HMs.
    static const uint16_t GG_TM_MOVES[] = {
        29, 269, 270, 100, 156, 113, 182, 164, 115, 91,
        261, 263, 280, 19, 69, 86, 525, 369, 231, 399,
        492, 157, 9, 404, 127, 398, 92, 161, 503, 339,
        7, 605, 347, 406, 8, 85, 53, 87, 200, 94,
        89, 120, 247, 583, 76, 126, 57, 63, 276, 355,
        59, 188, 72, 430, 58, 446, 6, 529, 138, 224,
    };

    // Sword/Shield Technical Machines  (PKHeX PersonalInfo8SWSH.MachineMovesTechnical)
    // Index == TM number: index 0 == TM00 (item 1230), index 1.. == TM01.. (item 328..).
    static const uint16_t SWSH_TM_MOVES[] = {
        5, 25, 6, 7, 8, 9, 19, 42, 63, 416,
        345, 76, 669, 83, 86, 91, 103, 113, 115, 219,
        120, 156, 157, 168, 173, 182, 184, 196, 202, 204,
        211, 213, 201, 240, 241, 258, 250, 251, 261, 263,
        129, 270, 279, 280, 286, 291, 311, 313, 317, 328,
        331, 333, 340, 341, 350, 362, 369, 371, 372, 374,
        384, 385, 683, 409, 419, 421, 422, 423, 424, 427,
        433, 472, 478, 440, 474, 490, 496, 506, 512, 514,
        521, 523, 527, 534, 541, 555, 566, 577, 580, 581,
        604, 678, 595, 598, 206, 403, 684, 693, 707, 784,
    };

    // Sword/Shield Technical Records  (PKHeX PersonalInfo8SWSH.MachineMovesRecord)
    // TR00..TR99 == items 1130..1229; index i == item (1130 + i).
    static const uint16_t SWSH_TR_MOVES[] = {
        14, 34, 53, 56, 57, 58, 59, 67, 85, 87,
        89, 94, 97, 116, 118, 126, 127, 133, 141, 161,
        164, 179, 188, 191, 200, 473, 203, 214, 224, 226,
        227, 231, 242, 247, 248, 253, 257, 269, 271, 276,
        285, 299, 304, 315, 322, 330, 334, 337, 339, 347,
        348, 349, 360, 370, 390, 394, 396, 398, 399, 402,
        404, 405, 406, 408, 411, 412, 413, 414, 417, 428,
        430, 437, 438, 441, 442, 444, 446, 447, 482, 484,
        486, 492, 500, 502, 503, 526, 528, 529, 535, 542,
        583, 599, 605, 663, 667, 675, 676, 706, 710, 776,
    };

    // Brilliant Diamond/Shining Pearl  (PKHeX PersonalInfo8BDSP.MachineMoves)
    // TM01..TM100. No TM00: index i == item (328 + i) for TM01-92; items 420-427
    // (repurposed HM ids) are TM93-100, continuing at index 92.
    static const uint16_t BDSP_TM_MOVES[] = {
        264, 337, 352, 347, 46, 92, 258, 339, 331, 526,
        241, 269, 58, 59, 63, 113, 182, 240, 202, 219,
        605, 76, 231, 85, 87, 89, 490, 91, 94, 247,
        280, 104, 115, 351, 53, 188, 201, 126, 317, 332,
        259, 263, 521, 156, 213, 168, 211, 285, 503, 315,
        355, 411, 412, 206, 362, 374, 451, 203, 406, 409,
        261, 405, 417, 153, 421, 371, 278, 416, 397, 148,
        444, 419, 86, 360, 14, 446, 244, 555, 399, 157,
        404, 214, 523, 398, 138, 447, 207, 365, 369, 164,
        430, 433, 15, 19, 57, 70, 432, 249, 127, 431,
    };

    // Scarlet/Violet  (PKHeX PersonalInfo9SV.MachineMoves)
    // Index == TM number: index 0 == TM000 (item 1230), index 1.. == TM001..
    // (items 328.., then 618-620, 690-693, then TM100-229 == items 2160-2289).
    static const uint16_t SV_TM_MOVES[] = {
        5, 36, 204, 313, 97, 189, 184, 182, 424, 422,
        423, 352, 67, 491, 512, 522, 60, 109, 168, 574,
        885, 884, 886, 451, 83, 263, 342, 332, 523, 506,
        555, 232, 129, 345, 196, 341, 317, 577, 488, 490,
        314, 500, 101, 374, 525, 474, 419, 203, 521, 241,
        240, 201, 883, 684, 473, 91, 331, 206, 280, 428,
        369, 421, 492, 706, 339, 403, 34, 7, 9, 8,
        214, 402, 486, 409, 115, 113, 350, 127, 337, 605,
        118, 447, 86, 398, 707, 156, 157, 269, 14, 776,
        191, 390, 286, 430, 399, 141, 598, 19, 285, 442,
        349, 408, 441, 164, 334, 404, 529, 261, 242, 271,
        710, 202, 396, 366, 247, 406, 446, 304, 257, 412,
        94, 484, 227, 57, 861, 53, 85, 583, 133, 347,
        270, 676, 226, 414, 179, 58, 604, 580, 678, 581,
        417, 126, 56, 59, 519, 518, 520, 528, 188, 89,
        444, 566, 416, 307, 308, 338, 200, 315, 411, 437,
        542, 433, 405, 63, 413, 394, 87, 370, 76, 434,
        796, 851, 46, 268, 114, 92, 328, 180, 356, 479,
        360, 282, 450, 162, 410, 679, 667, 333, 503, 535,
        669, 253, 264, 311, 803, 807, 812, 814, 809, 808,
        799, 802, 220, 244, 38, 283, 572, 915, 250, 330,
        916, 527, 813, 811, 482, 815, 297, 248, 797, 806,
        800, 675, 784, 319, 174, 912, 913, 914, 917, 918,
    };

    // Legends: Z-A  (PKHeX PersonalInfo9ZA.MachineMoves)
    // Listed in the SAME order as ItemStorage9ZA.TM, so index i maps directly to
    // that item list (no TM00). Item order: 328-419 (idx 0-91), 618-620 (92-94),
    // 690-693 (95-98), 2160 (99), 2162-2221 (100-159; note item 2161 is skipped).
    // The base game occupies indices 0-106; the DLC adds 107-159.
    static const uint16_t ZA_TM_MOVES[] = {
        29, 337, 473, 249, 46, 347, 92, 86, 812, 280,
        339, 157, 58, 424, 423, 113, 182, 612, 408, 583,
        422, 332, 9, 8, 242, 412, 129, 91, 7, 14,
        115, 104, 34, 400, 203, 317, 446, 126, 435, 331,
        352, 202, 19, 63, 282, 341, 97, 120, 196, 315,
        219, 414, 188, 434, 416, 38, 261, 442, 428, 248,
        421, 53, 94, 76, 444, 521, 85, 257, 89, 250,
        304, 83, 57, 247, 406, 710, 398, 523, 542, 334,
        404, 369, 417, 430, 164, 528, 231, 191, 390, 399,
        174, 605, 200, 18, 269, 56, 377, 127, 118, 441,
        527, 411, 526, 394, 59, 87, 370, 4, 263, 886,
        47, 491, 490, 488, 885, 6, 318, 325, 466, 246,
        259, 206, 305, 706, 102, 443, 138, 402, 509, 451,
        409, 458, 299, 814, 530, 815, 480, 524, 207, 330,
        252, 660, 799, 813, 13, 130, 161, 503, 333, 410,
        80, 669, 143, 90, 329, 800, 796, 307, 308, 338,
    };

    // FireRed/LeafGreen  (PKHeX PersonalInfo3.MachineMovesTechnical / MachineMovesHidden)
    // Gen 3 predates the 328.. item block: TM01..TM50 == items 289..338 and
    // HM01..HM08 == items 339..346. The same 50/8 ordering indexes the 58 TM/HM
    // compatibility bits in hmtm_g3.pkl that gen_learnsets.py reads.
    static const uint16_t FRLG_TM_MOVES[] = {
        264, 337, 352, 347, 46, 92, 258, 339, 331, 237,
        241, 269, 58, 59, 63, 113, 182, 240, 202, 219,
        218, 76, 231, 85, 87, 89, 216, 91, 94, 247,
        280, 104, 115, 351, 53, 188, 201, 126, 317, 332,
        259, 263, 290, 156, 213, 168, 211, 285, 289, 315,
    };
    static const uint16_t FRLG_HM_MOVES[] = {
        15, 19, 57, 70, 148, 249, 127, 291,
    };

    // Bounds-checked table read.
    static uint16_t idxMove(const uint16_t* table, size_t count, size_t index) {
        return index < count ? table[index] : 0;
    }

    uint16_t getTMMove(GameVersion group, uint16_t itemId) {
        switch (group) {
            // ---- FireRed/LeafGreen ----
            case GameVersion::FR:
            case GameVersion::LG:
            case GameVersion::FRLG: {
                constexpr size_t NT = sizeof(FRLG_TM_MOVES) / sizeof(FRLG_TM_MOVES[0]);
                constexpr size_t NH = sizeof(FRLG_HM_MOVES) / sizeof(FRLG_HM_MOVES[0]);
                if (itemId >= 289 && itemId <= 338) return idxMove(FRLG_TM_MOVES, NT, itemId - 289);  // TM01-50
                if (itemId >= 339 && itemId <= 346) return idxMove(FRLG_HM_MOVES, NH, itemId - 339);  // HM01-08
                return 0;
            }

            // ---- Let's Go, Pikachu!/Eevee! ----
            case GameVersion::GP:
            case GameVersion::GE:
            case GameVersion::GG: {
                constexpr size_t N = sizeof(GG_TM_MOVES) / sizeof(GG_TM_MOVES[0]);
                if (itemId >= 328 && itemId <= 387)
                    return idxMove(GG_TM_MOVES, N, itemId - 328);
                return 0;
            }

            // ---- Sword/Shield (TMs + Technical Records) ----
            case GameVersion::SW:
            case GameVersion::SH:
            case GameVersion::SWSH: {
                constexpr size_t NT = sizeof(SWSH_TM_MOVES) / sizeof(SWSH_TM_MOVES[0]);
                constexpr size_t NR = sizeof(SWSH_TR_MOVES) / sizeof(SWSH_TR_MOVES[0]);
                if (itemId >= 328 && itemId <= 419)   return idxMove(SWSH_TM_MOVES, NT, 1 + (itemId - 328));  // TM01-92
                if (itemId >= 618 && itemId <= 620)   return idxMove(SWSH_TM_MOVES, NT, 93 + (itemId - 618)); // TM93-95
                if (itemId >= 690 && itemId <= 693)   return idxMove(SWSH_TM_MOVES, NT, 96 + (itemId - 690)); // TM96-99
                if (itemId == 1230)                   return idxMove(SWSH_TM_MOVES, NT, 0);                   // TM00
                if (itemId >= 1130 && itemId <= 1229) return idxMove(SWSH_TR_MOVES, NR, itemId - 1130);       // TR00-99
                return 0;
            }

            // ---- Brilliant Diamond/Shining Pearl ----
            case GameVersion::BD:
            case GameVersion::SP:
            case GameVersion::BDSP: {
                constexpr size_t N = sizeof(BDSP_TM_MOVES) / sizeof(BDSP_TM_MOVES[0]);
                if (itemId >= 328 && itemId <= 419) return idxMove(BDSP_TM_MOVES, N, itemId - 328);        // TM01-92
                if (itemId >= 420 && itemId <= 427) return idxMove(BDSP_TM_MOVES, N, 92 + (itemId - 420)); // TM93-100
                return 0;
            }

            // ---- Scarlet/Violet ----
            case GameVersion::SL:
            case GameVersion::VL:
            case GameVersion::SV: {
                constexpr size_t N = sizeof(SV_TM_MOVES) / sizeof(SV_TM_MOVES[0]);
                if (itemId >= 328 && itemId <= 419)   return idxMove(SV_TM_MOVES, N, 1 + (itemId - 328));    // TM001-092
                if (itemId >= 618 && itemId <= 620)   return idxMove(SV_TM_MOVES, N, 93 + (itemId - 618));   // TM093-095
                if (itemId >= 690 && itemId <= 693)   return idxMove(SV_TM_MOVES, N, 96 + (itemId - 690));   // TM096-099
                if (itemId == 1230)                   return idxMove(SV_TM_MOVES, N, 0);                     // TM000
                if (itemId >= 2160 && itemId <= 2289) return idxMove(SV_TM_MOVES, N, 100 + (itemId - 2160)); // TM100-229
                return 0;
            }

            // ---- Legends: Z-A ----
            case GameVersion::ZA: {
                constexpr size_t N = sizeof(ZA_TM_MOVES) / sizeof(ZA_TM_MOVES[0]);
                if (itemId >= 328 && itemId <= 419)   return idxMove(ZA_TM_MOVES, N, itemId - 328);          // idx 0-91
                if (itemId >= 618 && itemId <= 620)   return idxMove(ZA_TM_MOVES, N, 92 + (itemId - 618));   // idx 92-94
                if (itemId >= 690 && itemId <= 693)   return idxMove(ZA_TM_MOVES, N, 95 + (itemId - 690));   // idx 95-98
                if (itemId == 2160)                   return idxMove(ZA_TM_MOVES, N, 99);                    // idx 99
                if (itemId >= 2162 && itemId <= 2221) return idxMove(ZA_TM_MOVES, N, 100 + (itemId - 2162)); // idx 100-159
                return 0;
            }

            // ---- Legends: Arceus teaches moves without TM items ----
            case GameVersion::PLA:
                return 0;

            default:
                return 0;
        }
    }
}
