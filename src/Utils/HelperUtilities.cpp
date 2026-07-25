#include <cstdint>
#include <random>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "Utils/HelperUtilities.h"
#include "Utils/StringHelpers.h"
#include "Utils/Logger.h"

namespace Utils {
    uint16_t readUInt16LittleEndian(const uint8_t* ptr) {
        return
        static_cast<uint16_t>(ptr[0]) |
        (static_cast<uint16_t>(ptr[1]) << 8);
    }

    int32_t readInt32LittleEndian(const uint8_t* ptr) {
        return
            static_cast<uint32_t>(ptr[0]) |
            (static_cast<uint32_t>(ptr[1]) << 8) |
            (static_cast<uint32_t>(ptr[2]) << 16) |
            (static_cast<uint32_t>(ptr[3]) << 24);
    }

    uint32_t readUInt32LittleEndian(const uint8_t* ptr) {
        return
            static_cast<uint32_t>(ptr[0]) |
            (static_cast<uint32_t>(ptr[1]) << 8) |
            (static_cast<uint32_t>(ptr[2]) << 16) |
            (static_cast<uint32_t>(ptr[3]) << 24);
    }

    uint64_t readUInt64LittleEndian(const uint8_t* ptr) {
        return
            static_cast<uint64_t>(ptr[0]) |
            (static_cast<uint64_t>(ptr[1]) << 8) |
            (static_cast<uint64_t>(ptr[2]) << 16) |
            (static_cast<uint64_t>(ptr[3]) << 24) |
            (static_cast<uint64_t>(ptr[4]) << 32) |
            (static_cast<uint64_t>(ptr[5]) << 40) |
            (static_cast<uint64_t>(ptr[6]) << 48) |
            (static_cast<uint64_t>(ptr[7]) << 56);
    }

    void writeUInt16LittleEndian(uint8_t* ptr, uint16_t value) {
        ptr[0] = static_cast<uint8_t>(value & 0xFF);
        ptr[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    void writeUInt32LittleEndian(uint8_t* ptr, uint32_t value) {
        ptr[0] = static_cast<uint8_t>(value & 0xFF);
        ptr[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        ptr[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        ptr[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }

    uint32_t rand32() noexcept {
        // Process-lifetime Mersenne Twister, seeded once from libnx's system tick counter
        // (armGetSystemTick, a u64). The high and low halves are folded together so all of the
        // tick's entropy reaches mt19937's 32-bit seed. mt19937 already yields the full 32-bit
        // range, so its output is returned directly. Used by the Pokemon creator for a mon's
        // PID / EncryptionConstant, where a unique-per-call value is all that's required.
        static std::mt19937 engine([]() -> std::mt19937::result_type {
#ifdef __SWITCH__
            const u64 tick = armGetSystemTick();
#else
            // Host builds (the round-trip harness, #50) have no system tick. The harness never
            // generates Pokemon, so this only has to compile and be distinct per run.
            const u64 tick = static_cast<u64>(std::random_device{}());
#endif
            return static_cast<std::mt19937::result_type>(tick ^ (tick >> 32));
        }());
        return static_cast<uint32_t>(engine());
    }

    // Title lookups go through the Switch's ns service, so they exist only on console. Deliberately
    // NOT stubbed for the host: a stub returning "UnknownGame" would let host code silently take a
    // wrong path, whereas an unresolved symbol says plainly that this belongs on the Switch (#50).
#ifdef __SWITCH__
    std::string getTitleVersion(u64 titleId)
    {
        NsApplicationControlData controlData;
        u64 controlDataSize = 0;
        Result result = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, &controlData, sizeof(controlData), &controlDataSize);
        if (R_FAILED(result))
        {
            printf("Failed to get application control data for TitleID: 0x%016lX (error: 0x%x)\n", titleId, result);
            return "";
        }

        const NacpStruct *nacp = (const NacpStruct *)&controlData.nacp;

        // display_version contains the version string like "1.0.1" or "1.3.2"
        if (nacp->display_version[0] != '\0')
        {
            return std::string(nacp->display_version);
        }

        return "";
    }
#endif  // __SWITCH__ (getTitleVersion)
}
