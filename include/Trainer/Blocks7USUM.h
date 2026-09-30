/**
 * GENERATED -- do not hand-edit. Ultra Sun/Ultra Moon save block table.
 *
 * Regenerate with `python tools/gen_blocktables.py`. Source: PKHeX SaveBlockAccessor7USUM.
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
#ifndef TRAINER_BLOCKS7USUM_H
#define TRAINER_BLOCKS7USUM_H

#include "Trainer/BlockTable.h"

namespace Trainer
{
    inline constexpr size_t SAVE_SIZE_7USUM = 0x06cc00;
    inline constexpr size_t BOX_COUNT_7USUM = 32;

    inline constexpr BlockEntry3DS BLOCKS_7USUM[] = {
        { 0x000000, 0x00e28 },  // 00 MyItem
        { 0x001000, 0x0007c },  // 01 Situation
        { 0x001200, 0x00014 },  // 02 RandomGroup
        { 0x001400, 0x000c0 },  // 03 MyStatus
        { 0x001600, 0x0061c },  // 04 PokePartySave
        { 0x001e00, 0x00e00 },  // 05 EventWork
        { 0x002c00, 0x00f78 },  // 06 ZukanData
        { 0x003c00, 0x00228 },  // 07 GtsData
        { 0x004000, 0x0030c },  // 08 UnionPokemon
        { 0x004400, 0x001fc },  // 09 Misc
        { 0x004600, 0x0004c },  // 10 FieldMenu
        { 0x004800, 0x00004 },  // 11 ConfigSave
        { 0x004a00, 0x00058 },  // 12 GameTime
        { 0x004c00, 0x005e6 },  // 13 BOX
        { 0x005200, 0x36600 },  // 14 BoxPokemon
        { 0x03b800, 0x0572c },  // 15 ResortSave
        { 0x041000, 0x00008 },  // 16 PlayTime
        { 0x041200, 0x01218 },  // 17 FieldMoveModelSave
        { 0x042600, 0x01a08 },  // 18 Fashion
        { 0x044200, 0x06408 },  // 19 JoinFestaPersonalSave
        { 0x04a800, 0x06408 },  // 20 JoinFestaPersonalSave
        { 0x050e00, 0x03998 },  // 21 JoinFestaDataSave
        { 0x054800, 0x00100 },  // 22 BerrySpot
        { 0x054a00, 0x00100 },  // 23 FishingSpot
        { 0x054c00, 0x10528 },  // 24 LiveMatchData
        { 0x065200, 0x00204 },  // 25 BattleSpotData
        { 0x065600, 0x00b60 },  // 26 PokeFinderSave
        { 0x066200, 0x03f50 },  // 27 MysteryGiftSave
        { 0x06a200, 0x00358 },  // 28 Record
        { 0x06a600, 0x00728 },  // 29 ValidationSave
        { 0x06ae00, 0x00200 },  // 30 GameSyncSave
        { 0x06b000, 0x00718 },  // 31 PokeDiarySave
        { 0x06b800, 0x001fc },  // 32 BattleInstSave
        { 0x06ba00, 0x00200 },  // 33 Sodateya
        { 0x06bc00, 0x00120 },  // 34 WeatherSave
        { 0x06be00, 0x001c8 },  // 35 QRReaderSaveData
        { 0x06c000, 0x00200 },  // 36 TurtleSalmonSave
        { 0x06c200, 0x0039c },  // 37 BattleFesSave
        { 0x06c600, 0x00400 },  // 38 FinderStudioSave
    };
    inline constexpr size_t BLOCK_COUNT_7USUM = sizeof(BLOCKS_7USUM) / sizeof(BlockEntry3DS);

    // Block indices the save layer reaches for, resolved from PKHeX's accessor/SAV classes.
    inline constexpr size_t BLOCK_ITEM_7USUM = 0;
    inline constexpr size_t BLOCK_BOX_LAYOUT_7USUM = 13;
    inline constexpr size_t BLOCK_STATUS_7USUM = 3;
    inline constexpr size_t BLOCK_MISC_7USUM = 9;
    inline constexpr size_t BLOCK_PARTY_7USUM = 4;
    inline constexpr size_t BLOCK_BOX_7USUM = 14;
}

#endif  // TRAINER_BLOCKS7USUM_H
