#include "Enums/SCTypeCode.h"

namespace Enums {
    /**
     * Byte width of an SCBlock's stored value, per PKHeX's SCTypeCode.GetTypeSize.
     *
     * These are SAVE-FORMAT widths, so every one is spelled with a fixed-width type. The table used
     * to say `sizeof(long)` for Int64 and `sizeof(int)`/`sizeof(short)` for Int32/Int16, which are
     * platform-dependent: `long` is 8 bytes under the Switch's LP64 toolchain but 4 under Windows
     * LLP64. That was correct on console by luck rather than intent, and it desynchronised the
     * block reader anywhere else -- an Int64 block would consume 4 bytes instead of 8, and since
     * the writer echoed the same wrong width the damage stayed invisible until the stream hit
     * something that couldn't parse, hundreds of KB later.
     *
     * Found by the round-trip harness (#50), which is exactly the class of bug it exists to catch.
     */
    size_t getTypeSize(SCTypeCode type)
    {
        switch(type)
        {
            case SCTypeCode::Bool3:  return sizeof(uint8_t);   // 1 byte, like the other bools
            case SCTypeCode::Byte:   return sizeof(uint8_t);
            case SCTypeCode::UInt16: return sizeof(uint16_t);
            case SCTypeCode::UInt32: return sizeof(uint32_t);
            case SCTypeCode::UInt64: return sizeof(uint64_t);
            case SCTypeCode::SByte:  return sizeof(int8_t);
            case SCTypeCode::Int16:  return sizeof(int16_t);
            case SCTypeCode::Int32:  return sizeof(int32_t);
            case SCTypeCode::Int64:  return sizeof(int64_t);

            case SCTypeCode::Single: return 4;   // IEEE-754 binary32, not sizeof(float)
            case SCTypeCode::Double: return 8;   // IEEE-754 binary64, not sizeof(double)

            default: return 0;
        }
    }
}
