#ifndef UTILS_GEN3TEXT_H
#define UTILS_GEN3TEXT_H
/**
 * Gen 3 stores names in a bespoke single-byte encoding, not ASCII: in the international games
 * 'A' is 0xBB, 'a' is 0xD5, '0' is 0xA1, the apostrophe is 0xB4 and the gender signs are
 * 0xB5/0xB6. Every Gen 3 name field goes through it -- the PK3 nickname and OT name, the trainer
 * name, the box names, and the nickname the cross-generation converter writes on a down-convert.
 *
 * ONE TABLE PAIR, ONE PLACE. Four independent subsets covering only space, digits, A-Z, a-z and
 * `! ? . -` is what this replaces, and each lost characters differently: a substituted '?' turned
 * a real FireRed FARFETCH'D into FARFETCH?D, a dropped character shortened the trainer name, and
 * a writer ended the name there. Add a character here, once.
 *
 * THERE ARE TWO TABLES AND THE LANGUAGE PICKS ONE, which is the whole reason every entry point
 * here takes a language byte. A byte value means a different glyph in a Japanese cartridge than
 * in an international one -- they are font maps, not a shared encoding -- so the SAME bytes read
 * two ways. PKHeX keys this on the record's own language (PK3.Nickname passes `Language` into
 * StringConverter3), never on the save it is sitting in, because a Japanese Pokemon traded into
 * an English game keeps its own bytes. Reading a Japanese record through the international table
 * does not fail loudly: 78 of the first 176 kana have no international slot at all, so a name is
 * truncated at the first of them and 337 of the 386 Gen 3 species' Japanese names come back
 * mangled or empty.
 *
 * GEN3_EN is PKHeX's StringConverter3.G3_EN: 247 of the 256 bytes carry a glyph, 0xF7-0xFE have
 * none, and 0xFF terminates. The kana in the unused slots are not a transcription slip -- the
 * international font ROM really does carry them there. GEN3_JP is its G3_JP, a full kana set
 * whose Latin block is FULL-WIDTH and whose terminator run is only 0xFC-0xFF.
 *
 * The one deliberate divergence from PKHeX, in both tables: the gender signs are stored here as
 * U+2642/U+2640, where PKHeX's G3_EN holds the half-width U+246D/U+246E and normalises them to
 * U+2642/U+2640 on every read and back on every write. Storing the normalised form and skipping
 * both conversions is the same behaviour with one step instead of three, and it makes the two
 * tables agree with each other (PKHeX's own G3_JP already holds U+2642/U+2640). Do not "restore"
 * the half-width pair without also adding the two conversions it exists to feed.
 */
#include <cstdint>

#include "Enums/LanguageID.h"

namespace Utils {

    /// Gen 3 name fields are terminated -- and padded -- with 0xFF, never with 0x00 (that is a space).
    inline constexpr uint8_t GEN3_TERMINATOR = 0xFF;

    /// The three bytes whose glyph is decided by the language rather than by the table alone.
    /// The quote pair is a role per gen3QuoteLeft/gen3QuoteRight; the apostrophe is where a curly
    /// one the player's keyboard produced lands, which is PKHeX's remap and not a cartridge fact.
    inline constexpr uint8_t GEN3_QUOTE_LEFT_BYTE = 0xB1;
    inline constexpr uint8_t GEN3_QUOTE_RIGHT_BYTE = 0xB2;
    inline constexpr uint8_t GEN3_APOSTROPHE_BYTE = 0xB4;

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

    /// Gen 3 byte -> UTF-16 code unit, Japanese cartridge. 0 marks a byte with no glyph (0xFC-0xFF).
    inline constexpr char16_t GEN3_JP[256] = {
        u'　', u'あ', u'い', u'う', u'え', u'お', u'か', u'き', u'く', u'け', u'こ', u'さ', u'し', u'す', u'せ', u'そ',  // 0
        u'た', u'ち', u'つ', u'て', u'と', u'な', u'に', u'ぬ', u'ね', u'の', u'は', u'ひ', u'ふ', u'へ', u'ほ', u'ま',  // 1
        u'み', u'む', u'め', u'も', u'や', u'ゆ', u'よ', u'ら', u'り', u'る', u'れ', u'ろ', u'わ', u'を', u'ん', u'ぁ',  // 2
        u'ぃ', u'ぅ', u'ぇ', u'ぉ', u'ゃ', u'ゅ', u'ょ', u'が', u'ぎ', u'ぐ', u'げ', u'ご', u'ざ', u'じ', u'ず', u'ぜ',  // 3
        u'ぞ', u'だ', u'ぢ', u'づ', u'で', u'ど', u'ば', u'び', u'ぶ', u'べ', u'ぼ', u'ぱ', u'ぴ', u'ぷ', u'ぺ', u'ぽ',  // 4
        u'っ', u'ア', u'イ', u'ウ', u'エ', u'オ', u'カ', u'キ', u'ク', u'ケ', u'コ', u'サ', u'シ', u'ス', u'セ', u'ソ',  // 5
        u'タ', u'チ', u'ツ', u'テ', u'ト', u'ナ', u'ニ', u'ヌ', u'ネ', u'ノ', u'ハ', u'ヒ', u'フ', u'ヘ', u'ホ', u'マ',  // 6
        u'ミ', u'ム', u'メ', u'モ', u'ヤ', u'ユ', u'ヨ', u'ラ', u'リ', u'ル', u'レ', u'ロ', u'ワ', u'ヲ', u'ン', u'ァ',  // 7
        u'ィ', u'ゥ', u'ェ', u'ォ', u'ャ', u'ュ', u'ョ', u'ガ', u'ギ', u'グ', u'ゲ', u'ゴ', u'ザ', u'ジ', u'ズ', u'ゼ',  // 8
        u'ゾ', u'ダ', u'ヂ', u'ヅ', u'デ', u'ド', u'バ', u'ビ', u'ブ', u'ベ', u'ボ', u'パ', u'ピ', u'プ', u'ペ', u'ポ',  // 9
        u'ッ', u'０', u'１', u'２', u'３', u'４', u'５', u'６', u'７', u'８', u'９', u'！', u'？', u'。', u'ー', u'・',  // A
        u'…', u'『', u'』', u'「', u'」', u'♂', u'♀', u'円', u'．', u'×', u'／', u'Ａ', u'Ｂ', u'Ｃ', u'Ｄ', u'Ｅ',  // B
        u'Ｆ', u'Ｇ', u'Ｈ', u'Ｉ', u'Ｊ', u'Ｋ', u'Ｌ', u'Ｍ', u'Ｎ', u'Ｏ', u'Ｐ', u'Ｑ', u'Ｒ', u'Ｓ', u'Ｔ', u'Ｕ',  // C
        u'Ｖ', u'Ｗ', u'Ｘ', u'Ｙ', u'Ｚ', u'ａ', u'ｂ', u'ｃ', u'ｄ', u'ｅ', u'ｆ', u'ｇ', u'ｈ', u'ｉ', u'ｊ', u'ｋ',  // D
        u'ｌ', u'ｍ', u'ｎ', u'ｏ', u'ｐ', u'ｑ', u'ｒ', u'ｓ', u'ｔ', u'ｕ', u'ｖ', u'ｗ', u'ｘ', u'ｙ', u'ｚ', u'►',  // E
        u'：', u'Ä', u'Ö', u'Ü', u'ä', u'ö', u'ü', u'↑', u'↓', u'←', u'→', u'＋', 0, 0, 0, 0,  // F
    };

    /// True when `languageId` names a cartridge that uses GEN3_JP. Every other value -- including
    /// one Gen 3 never had, which a down-convert should have clamped before it got here -- reads
    /// as international, exactly as PKHeX's `language == Japanese ? G3_JP : G3_EN` does.
    inline constexpr bool gen3IsJapanese(uint8_t languageId) noexcept {
        return languageId == static_cast<uint8_t>(Enums::LanguageID::Japanese);
    }

    /// THE QUOTE PAIR IS THE ONE PLACE THE FIVE INTERNATIONAL LANGUAGES DISAGREE, so it cannot come
    /// from the table. 0xB1/0xB2 are "open quote"/"close quote" as ROLES, and the cartridge draws
    /// whichever pair its language writes: English, Italian and Spanish use the curly pair, French
    /// guillemets, German a low-open/high-close pair whose closing glyph is the OTHER languages'
    /// opening one. That last one is why charToGen3 has to special-case U+201C rather than trust a
    /// table hit. PKHeX StringConverter3.GetQuoteLeft/GetQuoteRight.
    inline constexpr char16_t gen3QuoteLeft(uint8_t languageId) noexcept {
        switch (static_cast<Enums::LanguageID>(languageId)) {
        case Enums::LanguageID::English:
        case Enums::LanguageID::Italian:
        case Enums::LanguageID::Spanish:  return u'“';
        case Enums::LanguageID::French:   return u'«';
        case Enums::LanguageID::German:   return u'„';
        default:                          return u'『';  // Japanese, and anything unrecognised
        }
    }

    inline constexpr char16_t gen3QuoteRight(uint8_t languageId) noexcept {
        switch (static_cast<Enums::LanguageID>(languageId)) {
        case Enums::LanguageID::English:
        case Enums::LanguageID::Italian:
        case Enums::LanguageID::Spanish:  return u'”';
        case Enums::LanguageID::French:   return u'»';
        case Enums::LanguageID::German:   return u'“';
        default:                          return u'』';  // Japanese, and anything unrecognised
        }
    }

    /// Gen 3 byte -> UTF-16 code unit; 0 when that byte has no glyph. Callers test the terminator
    /// themselves (0xFF also reads as 0 here, but stopping at it is the caller's job, not ours --
    /// the byte after a terminator is not name data).
    inline constexpr char16_t gen3ToChar(uint8_t packedByte, uint8_t languageId) noexcept {
        if (packedByte == GEN3_QUOTE_LEFT_BYTE) return gen3QuoteLeft(languageId);
        if (packedByte == GEN3_QUOTE_RIGHT_BYTE) return gen3QuoteRight(languageId);
        return gen3IsJapanese(languageId) ? GEN3_JP[packedByte] : GEN3_EN[packedByte];
    }

    /// The full-width form of a half-width character, or the character unchanged.
    ///
    /// A JAPANESE CARTRIDGE HAS NO HALF-WIDTH LATIN AT ALL -- its alphabet and digits live at
    /// 0xBB-0xEF as Ａ-Ｚ/ａ-ｚ/０-９ -- so a Latin trainer name entered on one is STORED
    /// full-width. A real Japanese Ultra Moon record carries `Ｋｉａｓｔａ`, not `Kiasta`, and
    /// that is what has to round-trip. Without this, every name a Japanese player typed in the
    /// Roman alphabet would be refused by charToGen3 at its first letter.
    ///
    /// The two blocks map by the standard offsets: U+0020 to the ideographic space, and
    /// U+0021-U+007E to U+FF01-U+FF5E. Nothing else moves, so a character already full-width,
    /// kana, or accented comes back untouched.
    inline constexpr char16_t gen3ToFullWidth(char16_t character) noexcept {
        if (character == u' ') return u'　';
        if (character >= u'!' && character <= u'~') {
            return static_cast<char16_t>(character + 0xFEE0);
        }
        return character;
    }

    /// UTF-16 code unit -> Gen 3 byte, or GEN3_TERMINATOR when the character has no Gen 3 glyph.
    /// The scan is linear over 256 entries, which is nothing against name fields of 7-10 characters.
    ///
    /// Half-width Latin is retried full-width on a Japanese cartridge rather than converted up
    /// front, so the fallback can only ever rescue a character the table genuinely lacks -- a
    /// blanket conversion would also rewrite the handful the Japanese table does hold half-width.
    inline uint8_t charToGen3(char16_t character, uint8_t languageId) noexcept {
        if (character == 0) return GEN3_TERMINATOR;  // must precede the scan -- 0xF7+ hold 0, not a glyph
        // PKHeX's user-friendly remaps: a quote or apostrophe the player's keyboard produced that
        // is not this language's own. U+201C is tested BEFORE the table because German stores it
        // as the CLOSING byte while every other language's table holds it as the opening one.
        if (character == u'’') return GEN3_APOSTROPHE_BYTE;
        if (character == u'“') {
            return gen3QuoteRight(languageId) == u'“' ? GEN3_QUOTE_RIGHT_BYTE : GEN3_QUOTE_LEFT_BYTE;
        }
        const char16_t *table = gen3IsJapanese(languageId) ? GEN3_JP : GEN3_EN;
        for (int tableIndex = 0; tableIndex < 256; ++tableIndex) {
            if (table[tableIndex] == character) return static_cast<uint8_t>(tableIndex);
        }
        if (character == u'”' || character == u'』') return GEN3_QUOTE_RIGHT_BYTE;
        if (character == u'«' || character == u'„' || character == u'『') return GEN3_QUOTE_LEFT_BYTE;
        if (character == u'»') return GEN3_QUOTE_RIGHT_BYTE;
        if (gen3IsJapanese(languageId)) {
            const char16_t fullWidthCharacter = gen3ToFullWidth(character);
            if (fullWidthCharacter != character) return charToGen3(fullWidthCharacter, languageId);
        }
        return GEN3_TERMINATOR;
    }
}

#endif  // UTILS_GEN3TEXT_H
