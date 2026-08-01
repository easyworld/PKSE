#ifndef ENUMS_SC_TYPE_CODE_H
#define ENUMS_SC_TYPE_CODE_H

#include <cstdint>

#include "Utils/NXTypes.h"   // u8..s64; <switch.h> on console, plain typedefs off it

namespace Enums {
    enum class SCTypeCode
    {
        None = 0,

        Bool1 = 1, // False?
        Bool2 = 2, // True?
        Bool3 = 3, // Either? (Array boolean type)

        Object = 4,

        Array = 5,

        Byte = 8,
        UInt16 = 9,
        UInt32 = 10,
        UInt64 = 11,
        SByte = 12,
        Int16 = 13,
        Int32 = 14,
        Int64 = 15,
        Single = 16,
        Double = 17,
    };

    /// Gets the number of bytes occupied by a variable of a given type.
    size_t getTypeSize(SCTypeCode type);
}

#endif