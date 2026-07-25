#ifndef UTILS_BLOCK_H
#define UTILS_BLOCK_H

#include <vector>
#include <cstdint>

#include "Enums/SCTypeCode.h"

using namespace Enums;

namespace Save {

    struct Block {
        uint32_t key;
        SCTypeCode type;
        SCTypeCode sub_type = SCTypeCode::None; // Only for Array
        std::vector<uint8_t> data; // Empty for Bool types
    };

    bool tryReadBlock(const uint8_t* data, size_t data_size, uint32_t key, size_t& offset, Block& result);
    /**
     * Parse every SCBlock in `data`.
     *
     * Parsing STOPS at the first block that fails to decode, and everything after it is discarded.
     * That matters far more than it looks: serializeAllBlocks() re-emits only what was parsed, and
     * the save path writes that as the ENTIRE file — so a parse that stops early silently truncates
     * the user's save on the next write.
     *
     * `outConsumed` reports how many bytes were covered by successfully parsed blocks. Compare it
     * against data_size to find out whether the parse actually reached the end.
     */
    std::vector<Block> parseAllBlocks(const uint8_t* data, size_t data_size, size_t* outConsumed = nullptr);

    // Serialization functions
    size_t writeBlock(const Block& block, std::vector<uint8_t>& output);
    std::vector<uint8_t> serializeAllBlocks(const std::vector<Block>& blocks);
}

#endif