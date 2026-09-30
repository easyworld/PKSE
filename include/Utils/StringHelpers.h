#ifndef UTILS_STRING_HELPERS_H
#define UTILS_STRING_HELPERS_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace Utils {
    constexpr uint16_t TerminatorNull = 0u;

    // Equivalent to C# LoadString
    /// Loads characters into the result buffer and returns the count of characters loaded.
    int loadString(const uint8_t* data, size_t data_size, char16_t* result, size_t result_capacity);

    // Equivalent to C# GetString
    /// Converts Generation 7-Beluga encoded data to a decoded string.
    std::u16string getString(const uint8_t* data, size_t data_size);

    /// Converts a UTF-16 string to UTF-8
    std::string utf16ToUtf8(const std::u16string& utf16str);

    /// Converts a UTF-8 string to UTF-16. Covers the full BMP; astral code points are emitted as
    /// surrogate pairs. Malformed/truncated byte sequences are skipped. The inverse of utf16ToUtf8.
    std::u16string utf8ToUtf16(const std::string& utf8str);

    /// Uppercases a Game Boy-era name and strips everything that is not a letter, a digit or a
    /// gender sign, so a stored Gen 1/2 name can be compared with the modern species name for the
    /// same Pokemon. The two spell several species differently: the games write MR.MIME with a
    /// one-dot leader and no space where the modern table has "Mr. Mime", and FARFETCH'D with
    /// whichever of the two apostrophes the game used. Run BOTH sides through this before
    /// comparing. Shared by Gen 1 and Gen 2, which ask the identical question -- is this stored
    /// name a nickname, or just the species name? -- and must not answer it two different ways.
    std::u16string normalizeGameBoyName(const std::u16string& text);

    /// Uppercases the twenty-six unaccented Latin letters and leaves every other code unit exactly
    /// as it is -- accented letters, kana, hangul, the gender signs and CJK all pass through.
    ///
    /// That is what the games do to a species name before Generation 5: BULBASAUR, ALAKAZAM,
    /// SCIZOR. It is deliberately NOT normalizeGameBoyName(), which also strips everything it
    /// cannot uppercase -- run a Japanese name through that and nothing comes back, so two
    /// different Japanese names compare equal.
    std::u16string upperCaseLatinLetters(const std::u16string& text);

    /// Writes a UTF-16LE string into destination (up to maxChars code units), then
    /// zero-fills the rest of the data_size bytes (null terminator + padding).
    /// data_size is the full field width in bytes (e.g. 26 for a 13-slot name).
    void setString(uint8_t* destination, size_t data_size, const std::u16string& value, size_t maxChars);
}

#endif