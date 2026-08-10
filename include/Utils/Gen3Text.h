#pragma once
/**
 * Gen3Text.h - the Gen 3 (GBA) English character set, in ONE place.
 *
 * Gen 3 stores names in a bespoke single-byte encoding, not ASCII: 'A' is 0xBB, 'a' is 0xD5,
 * '0' is 0xA1, the apostrophe is 0xB4 and the gender signs are 0xB5/0xB6. Every Gen 3 name field
 * goes through it -- the PK3 nickname and OT name, the trainer name, the box names, and the
 * nickname the cross-gen converter writes on a down-convert.
 *
 * It lives here because it used to live in FOUR places (Pokemon3FRLG, Trainer3FRLG twice, and
 * Conversion/Convert), each an independent *subset* covering space, digits, A-Z, a-z and
 * `! ? . -` and nothing else. Anything past that subset was lost, and how it was lost depended
 * on which copy you hit: the PK3 reader substituted '?', so a real FireRed FARFETCH'D read back
 * as FARFETCH?D and both Nidoran read as NIDORAN?; the trainer reader dropped the character
 * outright; and the writers ended the name at it. Four tables that have to agree eventually
 * don't -- the same failure the sprite download lists had. Add a character here, once.
 *
 * The mapping is PKHeX's StringConverter3.G3_EN verbatim: 247 of the 256 bytes carry a glyph,
 * 0xF7-0xFE have none, and 0xFF terminates. The kana in the unused slots are not a transcription
 * slip -- the English font ROM really does carry them there. Japanese saves use a different
 * table (G3_JP) that PKSE does not implement, so this is English-only, as the reader always was.
 */
#include <cstdint>

namespace Utils {

    /// Gen 3 name fields are terminated -- and padded -- with 0xFF, never with 0x00 (that is a space).
    inline constexpr uint8_t GEN3_TERMINATOR = 0xFF;

    /// Gen 3 byte -> UTF-16 code unit. 0 marks a byte with no glyph (0xF7-0xFF).
    inline constexpr char16_t GEN3_EN[256] = {
        u' ', u'À', u'Á', u'Â', u'Ç', u'È', u'É', u'Ê', u'Ë', u'Ì', u'こ', u'Î', u'Ï', u'Ò', u'Ó', u'Ô',  // 0
        u'Œ', u'Ù', u'Ú', u'Û', u'Ñ', u'ß', u'à', u'á', u'ね', u'ç', u'è', u'é', u'ê', u'ë', u'ì', u'ま',  // 1
        u'î', u'ï', u'ò', u'ó', u'ô', u'œ', u'ù', u'ú', u'û', u'ñ', u'º', u'ª', u'⑩', u'&', u'+', u'あ',  // 2
        u'ぃ', u'ぅ', u'ぇ', u'ぉ', u'ゃ', u'=', u';', u'が', u'ぎ', u'ぐ', u'げ', u'ご', u'ざ', u'じ', u'ず', u'ぜ',  // 3
        u'ぞ', u'だ', u'ぢ', u'づ', u'で', u'ど', u'ば', u'び', u'ぶ', u'べ', u'ぼ', u'ぱ', u'ぴ', u'ぷ', u'ぺ', u'ぽ',  // 4
        u'っ', u'¿', u'¡', u'⒆', u'⒇', u'オ', u'カ', u'キ', u'ク', u'ケ', u'Í', u'%', u'(', u')', u'セ', u'ソ',  // 5
        u'タ', u'チ', u'ツ', u'テ', u'ト', u'ナ', u'ニ', u'ヌ', u'â', u'ノ', u'ハ', u'ヒ', u'フ', u'ヘ', u'ホ', u'í',  // 6
        u'ミ', u'ム', u'メ', u'モ', u'ヤ', u'ユ', u'ヨ', u'ラ', u'リ', u'↑', u'↓', u'←', u'＋', u'ヲ', u'ン', u'ァ',  // 7
        u'ィ', u'ゥ', u'ェ', u'ォ', u'⒅', u'<', u'>', u'ガ', u'ギ', u'グ', u'ゲ', u'ゴ', u'ザ', u'ジ', u'ズ', u'ゼ',  // 8
        u'ゾ', u'ダ', u'ヂ', u'ヅ', u'デ', u'ド', u'バ', u'ビ', u'ブ', u'ベ', u'ボ', u'パ', u'ピ', u'プ', u'ペ', u'ポ',  // 9
        u'ッ', u'0', u'1', u'2', u'3', u'4', u'5', u'6', u'7', u'8', u'9', u'!', u'?', u'.', u'-', u'･',  // A
        u'⑬', u'“', u'”', u'‘', u'\'', u'♂', u'♀', u'$', u',', u'⑧', u'/', u'A', u'B', u'C', u'D', u'E',  // B
        u'F', u'G', u'H', u'I', u'J', u'K', u'L', u'M', u'N', u'O', u'P', u'Q', u'R', u'S', u'T', u'U',  // C
        u'V', u'W', u'X', u'Y', u'Z', u'a', u'b', u'c', u'd', u'e', u'f', u'g', u'h', u'i', u'j', u'k',  // D
        u'l', u'm', u'n', u'o', u'p', u'q', u'r', u's', u't', u'u', u'v', u'w', u'x', u'y', u'z', u'►',  // E
        u':', u'Ä', u'Ö', u'Ü', u'ä', u'ö', u'ü', 0, 0, 0, 0, 0, 0, 0, 0, 0,  // F
    };

    /// Gen 3 byte -> UTF-16 code unit; 0 when that byte has no glyph. Callers test the terminator
    /// themselves (0xFF also reads as 0 here, but stopping at it is the caller's job, not ours --
    /// the byte after a terminator is not name data).
    inline char16_t gen3ToChar(uint8_t b) noexcept { return GEN3_EN[b]; }

    /// UTF-16 code unit -> Gen 3 byte, or GEN3_TERMINATOR when the character has no Gen 3 glyph.
    /// The scan is linear over 256 entries, which is nothing against name fields of 7-10 characters.
    inline uint8_t charToGen3(char16_t c) noexcept {
        if (c == u'’') c = u'\'';       // PKHeX's user-friendly remap: curly apostrophe -> straight
        if (c == 0) return GEN3_TERMINATOR;  // must precede the scan -- 0xF7+ hold 0, not a glyph
        for (int i = 0; i < 256; ++i) {
            if (GEN3_EN[i] == c) return static_cast<uint8_t>(i);
        }
        return GEN3_TERMINATOR;
    }
}
