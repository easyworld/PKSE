#include <cstdint>
#include <cstddef>

namespace Names {
    // Nature name lookup table - indexed by Nature ID (0-24)
    static const char* NATURE_NAMES[] = {
        "勤奋",    // 0 Hardy
        "怕寂寞",  // 1 Lonely
        "勇敢",    // 2 Brave
        "固执",    // 3 Adamant
        "顽皮",    // 4 Naughty
        "大胆",    // 5 Bold
        "坦率",    // 6 Docile
        "悠闲",    // 7 Relaxed
        "淘气",    // 8 Impish
        "乐天",    // 9 Lax
        "胆小",    // 10 Timid
        "急躁",    // 11 Hasty
        "认真",    // 12 Serious
        "爽朗",    // 13 Jolly
        "天真",    // 14 Naive
        "内敛",    // 15 Modest
        "慢吞吞",  // 16 Mild
        "冷静",    // 17 Quiet
        "害羞",    // 18 Bashful
        "马虎",    // 19 Rash
        "沉着",    // 20 Calm
        "温和",    // 21 Gentle
        "自大",    // 22 Sassy
        "慎重",    // 23 Careful
        "浮躁"     // 24 Quirky
    };

    constexpr size_t NATURE_NAMES_COUNT = 25;

    const char* getNatureName(uint8_t natureId) {
        if (natureId >= NATURE_NAMES_COUNT) {
            return "未知";
        }
        return NATURE_NAMES[natureId];
    }
}
