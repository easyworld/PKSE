#ifndef UTILS_KEYBOARD_H
#define UTILS_KEYBOARD_H

#include <string>

namespace Utils {
    /**
     * Switch software-keyboard (swkbd) wrapper.
     *
     * swkbd is a LIBRARY APPLET: the call blocks and PKSE is suspended for as long as the keyboard
     * owns the screen. Three rules follow, and all are load-bearing:
     *
     *  - **Call it only from a screen's update(), never from draw().** The UI loop runs
     *    update() -> draw() -> flush(), so at update() time no NanoVG frame is open. Calling it
     *    mid-draw would leave a half-built frame open across the suspend.
     *  - **At most ONE prompt per frame -- never chain them.** Launching a second prompt immediately
     *    after the first returns (both inside one update()) crashes on hardware: a library applet
     *    can't be torn down and relaunched back-to-back. For multi-field input (e.g. a date), use a
     *    single combined field, or spread the prompts across frames.
     *  - **Treat a cancel as "no change", not as an empty string.** Returning "" for a cancel would
     *    blank whatever the user was editing, which is the opposite of what they asked for.
     */
    struct KeyboardResult {
        bool accepted = false;   // false = cancelled or the applet failed; `text` is then empty
        std::string text;        // UTF-8, as typed (NOT yet validated against a game's encoding)
    };

    KeyboardResult promptText(const std::string& header, const std::string& guide,
                              const std::string& initial, int maxChars);

    /**
     * Show the numeric keypad, seeded with `initial`. Returns the parsed value clamped to
     * [minValue, maxValue]; `accepted` is false on cancel or if the text wasn't a number.
     */
    struct NumberResult {
        bool accepted = false;
        int value = 0;
    };
    NumberResult promptNumber(const std::string& header, int initial, int minValue, int maxValue);
}

#endif
