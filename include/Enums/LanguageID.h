#ifndef ENUMS_ENUMS_H
#define ENUMS_ENUMS_H

#include <cstdint>
#include <cstddef>

namespace Enums {
    /// Contiguous series Game Language IDs
    enum class LanguageID
    {
        /// Undefined Language ID, usually indicative of a value not being set.
        /// Gen5 Japanese In-game Trades happen to not have their Language value set, and express Language=0.
        Hacked,

        /// Japanese (日本語)
        Japanese,

        /// English (US/UK/AU)
        English,

        /// French (Français)
        French,

        /// Italian (Italiano)
        Italian,

        /// German (Deutsch)
        German,

        /// Unused Language ID
        /// Was reserved for Korean in Gen3 but never utilized.
        UNUSED_6,

        /// Spanish (Español)
        Spanish,

        /// Korean (한국어)
        Korean,

        /// Chinese Simplified (简体中文)
        ChineseSimplified,

        /// Chinese Traditional (繁體中文)
        ChineseTraditional
    };

    /// Short language name for a stored language id (indices match LanguageID above).
    inline const char* getLanguageName(uint8_t id) {
        static const char* const names[] = {
            "-", "日语", "英语", "法语", "意大利语", "德语",
            "-", "西班牙语", "韩语", "简体中文", "繁体中文"
        };
        return id < (sizeof(names) / sizeof(names[0])) ? names[id] : "-";
    }
}

#endif
