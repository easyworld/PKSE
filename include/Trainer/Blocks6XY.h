/**
 * GENERATED -- do not hand-edit. X/Y save block table.
 *
 * Regenerate with `python tools/gen_blocktables.py`. Source: PKHeX SaveBlockAccessor6XY.
 *
 * ONE GROUP, ONE TABLE. The four 3DS `main` formats share a row SHAPE and nothing else -- their
 * block counts, lengths and file sizes all differ, so a table serving more than one of them would
 * read one game's save with another's geometry.
 *
 * This group checksums with CRC16-CCITT. Gen 6 and Gen 7 use different algorithms of the same
 * width, and nothing in the data says which produced a given value, so the container that owns
 * this table owns that choice too.
 *
 * The block indices below are read out of PKHeX's own accessor and SAV classes rather than typed,
 * so an index cannot drift from the block it names. See BlockTable.h for the row type.
 */
#ifndef TRAINER_BLOCKS6XY_H
#define TRAINER_BLOCKS6XY_H

#include "Trainer/BlockTable.h"

namespace Trainer
{
    inline constexpr size_t SAVE_SIZE_6XY = 0x065600;
    inline constexpr size_t BOX_COUNT_6XY = 31;

    inline constexpr BlockEntry3DS BLOCKS_6XY[] = {
        { 0x000000, 0x002c8 },  // 00 Puff
        { 0x000400, 0x00b88 },  // 01 MyItem
        { 0x001000, 0x0002c },  // 02 ItemInfo (Select Bound Items)
        { 0x001200, 0x00038 },  // 03 GameTime
        { 0x001400, 0x00150 },  // 04 Situation
        { 0x001600, 0x00004 },  // 05 RandomGroup (rand seeds)
        { 0x001800, 0x00008 },  // 06 PlayTime
        { 0x001a00, 0x001c0 },  // 07 Fashion
        { 0x001c00, 0x000be },  // 08 Amie minigame records
        { 0x001e00, 0x00024 },  // 09 temp variables (u32 id + 32 u8)
        { 0x002000, 0x02100 },  // 10 FieldMoveModelSave
        { 0x004200, 0x00140 },  // 11 Misc
        { 0x004400, 0x00440 },  // 12 BOX
        { 0x004a00, 0x00574 },  // 13 BattleBox
        { 0x005000, 0x04e28 },  // 14 PSS1
        { 0x00a000, 0x04e28 },  // 15 PSS2
        { 0x00f000, 0x04e28 },  // 16 PSS3
        { 0x014000, 0x00170 },  // 17 MyStatus
        { 0x014200, 0x0061c },  // 18 PokePartySave
        { 0x014a00, 0x00504 },  // 19 EventWork
        { 0x015000, 0x006a0 },  // 20 ZukanData
        { 0x015800, 0x00644 },  // 21 hologram clips
        { 0x016000, 0x00104 },  // 22 UnionPokemon (Fused)
        { 0x016200, 0x00004 },  // 23 ConfigSave
        { 0x016400, 0x00420 },  // 24 Amie decoration stuff
        { 0x016a00, 0x00064 },  // 25 OPower
        { 0x016c00, 0x003f0 },  // 26 Strength Rock position (xyz float: 84 entries, 12bytes/entry)
        { 0x017000, 0x0070c },  // 27 Trainer PR Video
        { 0x017800, 0x00180 },  // 28 GtsData
        { 0x017a00, 0x00004 },  // 29 Packed Menu Bits
        { 0x017c00, 0x0000c },  // 30 PSS Profile Q&A (6*questions, 6*answer)
        { 0x017e00, 0x00048 },  // 31 Repel Info, (Swarm?) and other overworld info (roamer)
        { 0x018000, 0x00054 },  // 32 BOSS data fetch history (serial/mystery gift), 4byte intro & 20*4byte entries
        { 0x018200, 0x00644 },  // 33 Streetpass history
        { 0x018a00, 0x005c8 },  // 34 LiveMatchData/BattleSpotData
        { 0x019000, 0x002f8 },  // 35 MAC Address & Network Connection Logging (0x98 per entry, 5 entries)
        { 0x019400, 0x01b40 },  // 36 Dendou (Hall of Fame)
        { 0x01b000, 0x001f4 },  // 37 BattleHouse (Maison)
        { 0x01b200, 0x001f0 },  // 38 Sodateya (Daycare)
        { 0x01b400, 0x00216 },  // 39 TrialHouse (Battle Institute)
        { 0x01b800, 0x00390 },  // 40 BerryField
        { 0x01bc00, 0x01a90 },  // 41 MysteryGiftSave
        { 0x01d800, 0x00308 },  // 42 [SubE]vent Log
        { 0x01dc00, 0x00618 },  // 43 PokeDiarySave
        { 0x01e400, 0x0025c },  // 44 Record
        { 0x01e800, 0x00834 },  // 45 Friend Safari (0x15 per entry, 100 entries)
        { 0x01f200, 0x00318 },  // 46 SuperTrain
        { 0x01f600, 0x007d0 },  // 47 Unused (lmao)
        { 0x01fe00, 0x00c48 },  // 48 LinkInfo
        { 0x020c00, 0x00078 },  // 49 PSS usage info
        { 0x020e00, 0x00200 },  // 50 GameSyncSave
        { 0x021000, 0x00c84 },  // 51 PSS Icon (bool32 data present, 40x40 u16 pic, unused)
        { 0x021e00, 0x00628 },  // 52 ValidationSave (updateable Public Key for legal check api calls)
        { 0x022600, 0x34ad0 },  // 53 Box
        { 0x057200, 0x0e058 },  // 54 JPEG
    };
    inline constexpr size_t BLOCK_COUNT_6XY = sizeof(BLOCKS_6XY) / sizeof(BlockEntry3DS);

    // Block indices the save layer reaches for, resolved from PKHeX's accessor/SAV classes.
    inline constexpr size_t BLOCK_ITEM_6XY = 1;
    inline constexpr size_t BLOCK_BOX_LAYOUT_6XY = 12;
    inline constexpr size_t BLOCK_STATUS_6XY = 17;
    inline constexpr size_t BLOCK_MISC_6XY = 11;
    inline constexpr size_t BLOCK_PARTY_6XY = 18;
    inline constexpr size_t BLOCK_BOX_6XY = 53;
}

#endif  // TRAINER_BLOCKS6XY_H
