/**
 * Bank.h - Persistent cross-GAME Pokemon storage ("bank")
 *
 * A PKSM-style storage bank: boxes of Pokemon that live OUTSIDE any single save file,
 * persisted to the SD card under sdmc:/PKSE/bank. UNIFIED across all games: every slot
 * carries its own game-group tag + native (encrypted) per-gen bytes, so Pokemon from all
 * six titles coexist in one bank. Deposit is passive (store as-is, byte-in == byte-out) --
 * the bank never converts or mutates a stored mon. Only withdraw-INTO-a-save converts, and
 * that cross-gen conversion is the caller's job (see TrainerViewScreen), not the bank's.
 */
#ifndef TRAINER_BANK_H
#define TRAINER_BANK_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Pokemon/Pokemon.h"
#include "Enums/GameVersion.h"

namespace Trainer {
    class Bank {
    public:
        static constexpr size_t BANK_BOX_COUNT = 8;         // number of bank boxes
        static constexpr size_t BANK_SLOTS_PER_BOX = 30;    // 6x5 grid per box

        /// Constructs the unified bank and loads any existing on-SD contents. On first run it
        /// also migrates the legacy per-game bank files (gg/swsh/za/... _bank.dat), if present.
        Bank();

        /// (Re)loads the bank from its SD file, discarding any in-memory changes. Clears all
        /// slots first, so this doubles as the "discard changes" path (revert to on-disk state).
        void load();

        /// Writes the bank to its SD file (tagged, encrypted records). Returns true on success.
        bool save() const;

        /// True if the in-memory boxes differ from the last saved/loaded on-disk state.
        /// Used to prompt Save/Discard when leaving the storage view (PKSM-style).
        bool hasChanged() const;

        size_t boxCount() const noexcept { return BANK_BOX_COUNT; }
        size_t slotsPerBox() const noexcept { return BANK_SLOTS_PER_BOX; }

        /// Longest bank box name we store/accept (characters). Cosmetic; keeps the names section bounded.
        static constexpr size_t MAX_BOX_NAME_LEN = 24;

        /// Bank Pokemon storage [box][slot]; nullptr = empty (mirrors Trainer::boxes). Slots may
        /// hold mons from different games -- each entity knows its own getGameGroup().
        std::vector<std::array<std::unique_ptr<::Pokemon::Pokemon>, BANK_SLOTS_PER_BOX>> boxes;

        /// Optional per-box display names (parallel to `boxes`). Empty = use the default "Bank N"
        /// label. Persisted alongside the mons and included in hasChanged(), so a rename triggers
        /// the Save/Discard prompt on exit exactly like moving a Pokemon does.
        std::array<std::string, BANK_BOX_COUNT> boxNames;

        /// The label to show for a bank box: the user's name if set, else "Bank N" (1-indexed).
        std::string boxDisplayName(size_t box) const;

        /// Slots dropped by the last load() because their record didn't decode to a valid Pokemon
        /// (bad checksum, species 0, unknown tag). Non-zero means the bank file was damaged.
        size_t lastLoadRejects() const noexcept { return loadRejects; }

        /// Occupied slots whose bytes did not survive an encrypt->decrypt round trip during the
        /// last save(). The bank's contract is byte-in == byte-out, so non-zero means a PKSE bug.
        ///
        /// Reported rather than enforced ON PURPOSE. A failed bank save blocks leaving the storage
        /// view, so treating a verification miss as a save failure would trap the user in the
        /// UI over what may be a false positive. Writing proceeds; the anomaly is surfaced instead.
        size_t lastVerifyFailures() const noexcept { return verifyFailures; }

    private:
        std::string filePath() const;
        std::vector<uint8_t> serialize() const;    // full on-disk image of the current boxes
        void migrateLegacyBanks();                 // one-time import of the old per-group *_bank.dat files
        /// Re-parse a serialized image and compare each occupied slot back to the live Pokemon.
        /// Returns the number of slots that failed; also fills verifyFailures.
        size_t verifyImage(const std::vector<uint8_t>& image) const;
        mutable std::vector<uint8_t> savedImage;   // serialized image as of the last load()/save()
        mutable size_t verifyFailures = 0;
        size_t loadRejects = 0;
    };
}

#endif
