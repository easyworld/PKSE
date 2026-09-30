/**
 * GENERATED -- do not hand-edit. Sun/Moon save block table.
 *
 * Regenerate with `python tools/gen_blocktables.py`. Source: PKHeX SaveBlockAccessor7SM.
 *
 * ONE GROUP, ONE TABLE. The four 3DS `main` formats share a row SHAPE and nothing else -- their
 * block counts, lengths and file sizes all differ, so a table serving more than one of them would
 * read one game's save with another's geometry.
 *
 * This group checksums with CRC16Invert. Gen 6 and Gen 7 use different algorithms of the same
 * width, and nothing in the data says which produced a given value, so the container that owns
 * this table owns that choice too.
 *
 * The block indices below are read out of PKHeX's own accessor and SAV classes rather than typed,
 * so an index cannot drift from the block it names. See BlockTable.h for the row type.
 */
#ifndef TRAINER_BLOCKS7SM_H
#define TRAINER_BLOCKS7SM_H

#include "Trainer/BlockTable.h"

namespace Trainer
{
    inline constexpr size_t SAVE_SIZE_7SM = 0x06be00;
    inline constexpr size_t BOX_COUNT_7SM = 32;

    inline constexpr BlockEntry3DS BLOCKS_7SM[] = {
        { 0x000000, 0x00de0 },  // 00 MyItem
        { 0x000e00, 0x0007c },  // 01 Situation
        { 0x001000, 0x00014 },  // 02 RandomGroup
        { 0x001200, 0x000c0 },  // 03 MyStatus
        { 0x001400, 0x0061c },  // 04 PokePartySave
        { 0x001c00, 0x00e00 },  // 05 EventWork
        { 0x002a00, 0x00f78 },  // 06 ZukanData
        { 0x003a00, 0x00228 },  // 07 GtsData
        { 0x003e00, 0x00104 },  // 08 UnionPokemon
        { 0x004000, 0x00200 },  // 09 Misc
        { 0x004200, 0x00020 },  // 10 FieldMenu
        { 0x004400, 0x00004 },  // 11 ConfigSave
        { 0x004600, 0x00058 },  // 12 GameTime
        { 0x004800, 0x005e6 },  // 13 BOX
        { 0x004e00, 0x36600 },  // 14 BoxPokemon
        { 0x03b400, 0x0572c },  // 15 ResortSave
        { 0x040c00, 0x00008 },  // 16 PlayTime
        { 0x040e00, 0x01080 },  // 17 FieldMoveModelSave
        { 0x042000, 0x01a08 },  // 18 Fashion
        { 0x043c00, 0x06408 },  // 19 JoinFestaPersonalSave
        { 0x04a200, 0x06408 },  // 20 JoinFestaPersonalSave
        { 0x050800, 0x03998 },  // 21 JoinFestaDataSave
        { 0x054200, 0x00100 },  // 22 BerrySpot
        { 0x054400, 0x00100 },  // 23 FishingSpot
        { 0x054600, 0x10528 },  // 24 LiveMatchData
        { 0x064c00, 0x00204 },  // 25 BattleSpotData
        { 0x065000, 0x00b60 },  // 26 PokeFinderSave
        { 0x065c00, 0x03f50 },  // 27 MysteryGiftSave
        { 0x069c00, 0x00358 },  // 28 Record
        { 0x06a000, 0x00728 },  // 29 ValidationSave
        { 0x06a800, 0x00200 },  // 30 GameSyncSave
        { 0x06aa00, 0x00718 },  // 31 PokeDiarySave
        { 0x06b200, 0x001fc },  // 32 BattleInstSave
        { 0x06b400, 0x00200 },  // 33 Sodateya
        { 0x06b600, 0x00120 },  // 34 WeatherSave
        { 0x06b800, 0x001c8 },  // 35 QRReaderSaveData
        { 0x06ba00, 0x00200 },  // 36 TurtleSalmonSave
    };
    inline constexpr size_t BLOCK_COUNT_7SM = sizeof(BLOCKS_7SM) / sizeof(BlockEntry3DS);

    // Block indices the save layer reaches for, resolved from PKHeX's accessor/SAV classes.
    inline constexpr size_t BLOCK_ITEM_7SM = 0;
    inline constexpr size_t BLOCK_BOX_LAYOUT_7SM = 13;
    inline constexpr size_t BLOCK_STATUS_7SM = 3;
    inline constexpr size_t BLOCK_MISC_7SM = 9;
    inline constexpr size_t BLOCK_PARTY_7SM = 4;
    inline constexpr size_t BLOCK_BOX_7SM = 14;
}

#endif  // TRAINER_BLOCKS7SM_H
