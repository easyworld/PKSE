#include "Utils/Keyboard.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <switch.h>

#include "Utils/Logger.h"

namespace Utils
{
    namespace
    {
        // swkbd's length cap is in characters, but the output buffer it fills is UTF-8, where one
        // character costs up to 4 bytes. Sizing the buffer from the character cap alone would
        // truncate any non-ASCII name mid-sequence.
        constexpr int BYTES_PER_CHARACTER = 4;

        /**
         * Common swkbd plumbing. Returns false on cancel OR on applet failure -- the caller treats
         * both as "no change", which is right either way: a failed applet must not be allowed to
         * overwrite the user's existing text with an empty string.
         */
        bool runKeyboard(SwkbdType type, const std::string &header, const std::string &guide,
                         const std::string &initial, int maxChars, std::string &out)
        {
            if (maxChars <= 0)
                return false;

            SwkbdConfig keyboardConfig;
            Result resultCode = swkbdCreate(&keyboardConfig, 0);
            if (R_FAILED(resultCode))
            {
                logErrorToFile("swkbdCreate failed");
                return false;
            }

            swkbdConfigMakePresetDefault(&keyboardConfig);
            swkbdConfigSetType(&keyboardConfig, type);
            swkbdConfigSetStringLenMax(&keyboardConfig, static_cast<u32>(maxChars));
            swkbdConfigSetStringLenMin(&keyboardConfig, 0); // allow clearing the field entirely
            if (!header.empty())
                swkbdConfigSetHeaderText(&keyboardConfig, header.c_str());
            if (!guide.empty())
                swkbdConfigSetGuideText(&keyboardConfig, guide.c_str());
            if (!initial.empty())
                swkbdConfigSetInitialText(&keyboardConfig, initial.c_str());

            // +1 for the NUL the applet writes.
            std::vector<char> buffer(static_cast<size_t>(maxChars) * BYTES_PER_CHARACTER + 1, '\0');
            resultCode = swkbdShow(&keyboardConfig, buffer.data(), buffer.size());
            swkbdClose(&keyboardConfig);

            if (R_FAILED(resultCode))
                return false;  // cancelled (the common case) or failed
            buffer.back() = '\0'; // belt and braces before constructing
            out.assign(buffer.data());
            return true;
        }
    }

    KeyboardResult promptText(const std::string &header, const std::string &guide,
                              const std::string &initial, int maxChars)
    {
        KeyboardResult result;
        result.accepted = runKeyboard(SwkbdType_QWERTY, header, guide, initial, maxChars, result.text);
        if (!result.accepted)
            result.text.clear();
        return result;
    }

    NumberResult promptNumber(const std::string &header, int initial, int minValue, int maxValue)
    {
        NumberResult result;
        if (minValue > maxValue)
            return result;

        // Width the field to the largest value that is actually allowed, so the keypad can't be
        // used to type a number the caller would only have to reject afterwards.
        const int digits = static_cast<int>(std::to_string(std::max(std::abs(minValue), std::abs(maxValue))).size());

        std::string text;
        if (!runKeyboard(SwkbdType_NumPad, header, std::to_string(minValue) + " - " + std::to_string(maxValue),
                         std::to_string(initial), digits, text))
            return result;

        // An empty field is a deliberate "no change" rather than 0 -- otherwise cancelling by
        // clearing the field would silently zero an item count.
        if (text.empty())
            return result;

        errno = 0;
        char *end = nullptr;
        const long newValue = std::strtol(text.c_str(), &end, 10);
        if (end == text.c_str() || errno == ERANGE)
            return result; // not a number at all

        result.value = static_cast<int>(std::clamp<long>(newValue, minValue, maxValue));
        result.accepted = true;
        return result;
    }
}
