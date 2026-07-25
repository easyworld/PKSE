#include <cstdint>
#include <cstddef>
#include <cstring>

#include "Save/Block.h"
#include "Utils/HelperUtilities.h"
#include "Utils/SCXorShift32.h"
#include "Enums/SCTypeCode.h"

using namespace Utils;
using namespace Enums;

namespace Save {
    // Reads a single block (ported from ReadFromOffset, with bound checks; returns true on success)
    bool tryReadBlock(const uint8_t* data, size_t data_size, uint32_t key, size_t& offset, Block& result) {
        if (offset >= data_size) return false;

        SCXorShift32 xk(key);

        result.key = key;
        result.type = static_cast<SCTypeCode>(data[offset] ^ xk.Next());
        ++offset;

        if (offset > data_size) return false; // Bound check after increment

        switch (result.type) {
            case SCTypeCode::Bool1:
            case SCTypeCode::Bool2:
            case SCTypeCode::Bool3:
    #ifdef DEBUG
                assert(result.type != SCTypeCode::Bool3); // As in original
    #endif
                return true; // No data

            case SCTypeCode::Object: {
                if (offset + 4 > data_size) return false;
                uint32_t num_bytes = readUInt32LittleEndian(data + offset) ^ xk.Next32();
                offset += 4;

                if (offset + num_bytes > data_size) return false;
                result.data.resize(num_bytes);
                std::memcpy(result.data.data(), data + offset, num_bytes);
                offset += num_bytes;

                for (size_t i = 0; i < num_bytes; ++i) {
                    result.data[i] ^= xk.Next();
                }
                return true;
            }

            case SCTypeCode::Array: {
                if (offset + 4 > data_size) return false;
                uint32_t num_entries = readUInt32LittleEndian(data + offset) ^ xk.Next32();
                offset += 4;

                if (offset >= data_size) return false;
                result.sub_type = static_cast<SCTypeCode>(data[offset] ^ xk.Next());
                ++offset;

                size_t elem_size = getTypeSize(result.sub_type);
                size_t num_bytes = num_entries * elem_size;

                if (offset + num_bytes > data_size) return false;
                result.data.resize(num_bytes);
                std::memcpy(result.data.data(), data + offset, num_bytes);
                offset += num_bytes;

                for (size_t i = 0; i < num_bytes; ++i) {
                    result.data[i] ^= xk.Next();
                }

    #ifdef DEBUG
                EnsureArrayIsSane(result.sub_type, result.data);
    #endif
                return true;
            }

            default: { // Single value
                size_t num_bytes = getTypeSize(result.type);
                if (num_bytes == 0 || offset + num_bytes > data_size) return false;

                result.data.resize(num_bytes);
                std::memcpy(result.data.data(), data + offset, num_bytes);
                offset += num_bytes;

                for (size_t i = 0; i < num_bytes; ++i) {
                    result.data[i] ^= xk.Next();
                }
                return true;
            }
        }
    }

    // Main function to parse all blocks into an array (vector) for manual searching
    std::vector<Block> parseAllBlocks(const uint8_t* data, size_t data_size, size_t* outConsumed) {
        std::vector<Block> blocks;
        size_t offset = 0;
        size_t consumed = 0;   // end of the last SUCCESSFULLY parsed block

        while (offset + 4 <= data_size) { // Need at least key
            uint32_t key = readUInt32LittleEndian(data + offset);
            offset += 4;

            Block b;
            if (!tryReadBlock(data, data_size, key, offset, b)) {
                // Invalid block: stop. Callers must check outConsumed — everything from here on is
                // dropped, and a later serialize would write a short file. See the header.
                break;
            }
            blocks.push_back(b);
            consumed = offset;
        }

        if (outConsumed) *outConsumed = consumed;
        return blocks;
    }

    // Writes a single block to output buffer (returns bytes written)
    size_t writeBlock(const Block& block, std::vector<uint8_t>& output) {
        size_t startSize = output.size();

        SCXorShift32 xk(block.key);

        // Write the key (not XORed)
        uint8_t keyBytes[4];
        writeUInt32LittleEndian(keyBytes, block.key);
        output.insert(output.end(), keyBytes, keyBytes + 4);

        // Write the type (XORed)
        output.push_back(static_cast<uint8_t>(block.type) ^ xk.Next());

        switch (block.type) {
            case SCTypeCode::Bool1:
            case SCTypeCode::Bool2:
            case SCTypeCode::Bool3:
                // No data for bools
                break;

            case SCTypeCode::Object: {
                // Write size (XORed)
                uint32_t numBytes = block.data.size();
                uint32_t encryptedSize = numBytes ^ xk.Next32();
                uint8_t sizeBytes[4];
                writeUInt32LittleEndian(sizeBytes, encryptedSize);
                output.insert(output.end(), sizeBytes, sizeBytes + 4);

                // Write data (XORed)
                for (size_t i = 0; i < numBytes; ++i) {
                    output.push_back(block.data[i] ^ xk.Next());
                }
                break;
            }

            case SCTypeCode::Array: {
                // Calculate number of entries
                size_t elemSize = getTypeSize(block.sub_type);
                uint32_t numEntries = (elemSize > 0) ? (block.data.size() / elemSize) : 0;

                // Write entry count (XORed)
                uint32_t encryptedCount = numEntries ^ xk.Next32();
                uint8_t countBytes[4];
                writeUInt32LittleEndian(countBytes, encryptedCount);
                output.insert(output.end(), countBytes, countBytes + 4);

                // Write sub_type (XORed)
                output.push_back(static_cast<uint8_t>(block.sub_type) ^ xk.Next());

                // Write data (XORed)
                for (size_t i = 0; i < block.data.size(); ++i) {
                    output.push_back(block.data[i] ^ xk.Next());
                }
                break;
            }

            default: {
                // Single value type
                for (size_t i = 0; i < block.data.size(); ++i) {
                    output.push_back(block.data[i] ^ xk.Next());
                }
                break;
            }
        }

        return output.size() - startSize;
    }

    // Serializes all blocks back to binary format
    std::vector<uint8_t> serializeAllBlocks(const std::vector<Block>& blocks) {
        std::vector<uint8_t> output;

        for (const auto& block : blocks) {
            writeBlock(block, output);
        }

        return output;
    }
}