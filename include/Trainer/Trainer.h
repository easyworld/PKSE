/**
 * Trainer.h - Base Trainer/Save File Data Management
 *
 * This file defines the abstract base Trainer class and related structures for
 * managing Pokemon save file data. This base class provides a unified interface for accessing trainer data
 * across all generations.
 *
 * Derived classes (Trainer7LGPE, Trainer8SWSH, etc.) implement generation-specific data
 * formats, encryption, and Pokemon storage.
 */

#ifndef TRAINER_TRAINER_H
#define TRAINER_TRAINER_H

#include <cstdint>
#include <vector>
#include <span>
#include <array>
#include <memory>
#include <string>

#include "Save/Block.h"
#include "Utils/Logger.h"
#include "Utils/HelperUtilities.h"
#include "Utils/StringHelpers.h"
#include "Trainer/Inventory.h"
#include "Pokemon/Pokemon.h"
#include "Enums/GameVersion.h"

using namespace Save;
using namespace Utils;
using namespace Enums;

namespace Trainer {

    // Pokemon storage constants (common across generations)
    constexpr size_t MAX_PARTY_SLOTS = 6;  // Maximum Pokemon in party
    constexpr size_t BOX_SLOTS = 30;       // Pokemon per box (6x5 grid)

    // ========================================
    // Name Lookup Functions
    // ========================================

    /// Converts a Species ID to its name string
    const char* getSpeciesName(uint16_t speciesId);

    /// Converts an Item ID to its name string
    const char* getItemName(uint16_t itemId);

    /// Gets total number of items
    size_t getItemCount();

    /// Converts a Nature ID to its name string
    const char* getNatureName(uint8_t natureId);

    /// Converts an Ability ID to its name string
    const char* getAbilityName(uint16_t abilityId);
}

namespace Trainer {
    /**
     * Trainer - Abstract base class for save file data
     *
     * This class provides the foundation for all generation-specific Trainer classes.
     * It defines the common interface that all save file formats must implement,
     * while allowing each generation to handle its own data layout, encryption, and
     * Pokemon storage.
     *
     * Common Data (stored in base class):
     * - Trainer information (ID, name, money)
     * - Item inventory
     * - Box names
     * - Save file blocks
     *
     * Generation-Specific Data (implemented in derived classes):
     * - Party Pokemon storage (Pokemon7LGPE vs Pokemon8SWSH vs Pokemon9LZA)
     * - Box Pokemon storage
     * - Block key constants
     * - Encryption methods
     *
     * Usage Pattern:
     * 1. Derived class receives save file blocks
     * 2. Constructor parses blocks and populates data
     * 3. User modifies Pokemon, items, or trainer info
     * 4. Update methods serialize changes back to blocks
     * 5. Blocks are re-encrypted and saved to file
     */
    class Trainer
    {
    protected:
        /**
         * All save file blocks for re-serialization.
         * Blocks contain encrypted data segments identified by key values.
         */
        std::vector<Block> blocks;

    public:
        // ========================================
        // Common Trainer Data
        // ========================================

        /// Trainer name (UTF-8 string)
        std::string trainerName;

        /// Money/currency amount
        uint32_t money;

        /// Trainer ID (32-bit format: SID16 << 16 | TID16)
        uint32_t ID32;

        /// Trainer gender (0 = Male, 1 = Female), read from the save. Stamped as a created mon's OT
        /// gender so it matches the current trainer -- Gen 3 flags a mismatch as "Apparently met".
        uint8_t trainerGender = 0;

        /// Trainer ID (16-bit visible ID)
        uint16_t TID16;

        /// Secret ID (16-bit hidden ID)
        uint16_t SID16;

        /// Display Trainer ID (Gen 7+)
        uint32_t TID;

        /// Display Secret ID (Gen 7+)
        uint32_t SID;

        /// Save file revision (DLC version detection)
        /// 0 = Base game, higher values indicate DLC/updates
        int saveRevision = 0;

        /// Human-readable save revision string (e.g., "Base", "IoA", "CT", "MD")
        std::string saveRevisionString = "Base";

        /// Inferred game version string (e.g., "v1.0", "v1.3", "v2.0")
        /// Based on save revision and known version mappings
        std::string gameVersionString = "";

        /// Items organized by pouch type (Medicine, Balls, etc.)
        std::vector<std::vector<InventoryItem>> items;

        /// Names of each box (UTF-8 strings)
        std::vector<std::string> boxNames;

        /// Storage box the game/editor should open on. Persisted for the games that store it
        /// (SV / Z-A "U32 Box Index" block); left 0 for games PKSE doesn't round-trip it for.
        uint8_t currentBox = 0;
        uint8_t getCurrentBox() const noexcept { return currentBox; }
        void setCurrentBox(uint8_t box) noexcept { currentBox = box; }

        /// Party Pokemon (1-6 Pokemon) - stored polymorphically
        std::vector<std::unique_ptr<::Pokemon::Pokemon>> party;

        /// Box Pokemon storage [box_index][slot_index] - stored polymorphically
        /// nullptr = empty slot
        std::vector<std::array<std::unique_ptr<::Pokemon::Pokemon>, BOX_SLOTS>> boxes;

        // ========================================
        // Constructors and Destructor
        // ========================================

        /**
         * Constructs a Trainer object from save file blocks.
         * Derived classes call this constructor to initialize common data.
         *
         * @param blocks Save file blocks parsed from the save file
         */
        explicit Trainer(std::vector<Block> blocks) : blocks(std::move(blocks)) {}

        /// Virtual destructor to ensure proper cleanup in derived classes
        virtual ~Trainer() = default;

        // Delete copy operations to prevent accidental copies of save data
        Trainer(const Trainer&) = delete;
        Trainer& operator=(const Trainer&) = delete;

        // Allow move operations for efficient transfers
        Trainer(Trainer&&) noexcept = default;
        Trainer& operator=(Trainer&&) noexcept = default;

        // ========================================
        // Pure Virtual Methods (Must Implement)
        // ========================================

        /**
         * Updates the party block with modified Pokemon data.
         * Each generation implements this with generation-specific encryption.
         */
        virtual void updatePartyBlock() = 0;

        /**
         * Updates the box blocks with modified Pokemon data.
         * Each generation implements this with generation-specific encryption.
         */
        virtual void updateBoxBlock() = 0;

        /**
         * Updates the item block with modified inventory data.
         * Item structure varies slightly between generations.
         */
        virtual void updateItemBlock() = 0;

        /**
         * Max number of item slots pouch `pouch` can hold, for the add-item flow. Default 0
         * means "appending unsupported" (the UI uses a large sentinel for the id-indexed games, which
         * never append). Legends: Arceus overrides this: its pouches are fixed-capacity packed lists,
         * and the general-items bag grows with the player's Satchel Upgrades (as in PKHeX PlayerBag8a).
         */
        virtual size_t getItemPouchCapacity(int pouch) const { return 0; }

        /**
         * True if items are stored ID-INDEXED (count at itemId * stride: BDSP / S-V / Z-A), false if
         * SLOT-BASED (a packed list of {itemId,count}: FRLG / LGPE / SWSH / LA). This decides how the
         * editor removes or retypes an item, because the two models save opposite ways:
         *   - slot-based: erase the entry / reassign its itemId (the region is rewritten each save);
         *   - id-indexed: set count 0 but KEEP the entry so its id-slot is written to 0 (updateItemBlock
         *     only touches ids still present, so an erased entry would ghost the old count).
         */
        virtual bool itemsAreIdIndexed() const { return false; }

        /**
         * Creates a blank, game-accepted Pokemon entity in this trainer's generation format.
         *
         * Synthesizes a zeroed *decrypted* buffer of the generation's box/party slot size,
         * encrypts it with seed/EC 0, then constructs the generation's Pokemon entity from those
         * encrypted bytes (the ctor decrypts them straight back to zeros). The result is a
         * species-0, Sanity-0, checksum-valid entity — the clean starting point the creator fills
         * in (species, PID/EC, level, ...). Mirrors each generation's own empty-box-slot encrypted
         * blank in updateBoxBlock(); raw zeros must never be handed to a ctor (they decrypt to a
         * Bad Egg). Dispatch is by trainer subclass, so no RTTI is needed.
         *
         * @return A generation-correct blank entity behind the base unique_ptr contract.
         */
        virtual std::unique_ptr<::Pokemon::Pokemon> createBlankPokemon() const = 0;

        /**
         * Gets the number of boxes available in this generation.
         * @return Number of boxes (e.g., 32 for Sword/Shield, 40 for Let's Go)
         */
        virtual size_t getBoxCount() const noexcept = 0;

        /**
         * Gets the number of slots per box in this generation.
         * @return Slots per box (e.g., 30 for Sword/Shield, 25 for Let's Go)
         */
        virtual size_t getSlotsPerBox() const noexcept = 0;

        /**
         * Gets the number of Pokemon currently in the party.
         * @return Party size (0-6)
         */
        virtual size_t getPartySize() const noexcept = 0;

        /**
         * Gets the game group for this trainer type.
         * Used for type dispatch without RTTI (required for Nintendo Switch builds).
         * @return GameVersion group (GG, SWSH, etc.)
         */
        virtual GameVersion getGameGroup() const noexcept = 0;

        /**
         * Checks if a Pokemon at the given storage location is the Partner/Starter Pokemon.
         * Only applicable to Let's Go games where the Partner Pikachu/Eevee has special status.
         * @param boxIndex Box index (0-based)
         * @param slotIndex Slot index within the box (0-based)
         * @return true if this Pokemon is the Partner/Starter Pokemon
         */
        virtual bool isStarterPokemon(size_t boxIndex, size_t slotIndex) const noexcept {
            (void)boxIndex;
            (void)slotIndex;
            return false;  // Default: no starter tracking for most games
        }

        /**
         * Gets the party position of a Pokemon at the given storage location.
         * For games like Let's Go that use index-based party systems.
         * @param boxIndex Box index (0-based)
         * @param slotIndex Slot index within the box (0-based)
         * @return Party position (1-6) if in party, 0 if not in party
         */
        virtual int getPartyPosition(size_t boxIndex, size_t slotIndex) const noexcept {
            (void)boxIndex;
            (void)slotIndex;
            return 0;  // Default: no index-based party tracking for most games
        }

        /**
         * Checks if a party Pokemon at the given index is the Partner/Starter Pokemon.
         * Only applicable to Let's Go games where the Partner Pikachu/Eevee has special status.
         * @param partyIndex Party index (0-based, 0-5)
         * @return true if this Pokemon is the Partner/Starter Pokemon
         */
        virtual bool isPartyPokemonStarter(size_t partyIndex) const noexcept {
            (void)partyIndex;
            return false;  // Default: no starter tracking for most games
        }

        /**
         * Mirrors an edited party member's two representations so neither clobbers the other on save.
         * Let's Go stores each party member as BOTH a box/storage slot and an independent party copy,
         * and updateBoxBlock() overlays the party copy onto the box slot ("party wins"). Without this,
         * an edit made to a party member's box slot is silently lost on save. No-op for gens where the
         * party and boxes are separate save regions (SWSH/LZA).
         * @param boxIndex Box of the edited slot
         * @param slotIndex Slot within the box
         */
        virtual void mirrorPartyMemberFromBox(size_t boxIndex, size_t slotIndex) {
            (void)boxIndex; (void)slotIndex;
        }

        /**
         * Mirrors an edited party copy back into its box/storage slot (the display copy) so the two
         * representations stay identical. No-op where party and boxes are separate regions.
         * @param partyIndex Party slot (0-based)
         */
        virtual void mirrorPartyMemberFromParty(size_t partyIndex) {
            (void)partyIndex;
        }

        // ========================================
        // Common Methods
        // ========================================

        /**
         * Gets the blocks for serialization back to save file.
         * @return Const reference to blocks vector
         */
        const std::vector<Block>& getBlocks() const {
            return blocks;
        }

        /**
         * Swaps the Pokemon occupying two box slots. Derived classes may override to also
         * update generation-specific bookkeeping (e.g. LGPE party/starter storage pointers
         * that reference slots by index).
         */
        virtual void swapBoxSlots(size_t b1, size_t s1, size_t b2, size_t s2) {
            if (b1 >= boxes.size() || b2 >= boxes.size()) return;
            if (s1 >= BOX_SLOTS || s2 >= BOX_SLOTS) return;
            std::swap(boxes[b1][s1], boxes[b2][s2]);
        }

        /**
         * Re-pack storage after a slot is vacated, for games that store boxes as a GAPLESS list.
         *
         * Only Let's Go needs this: its 1000 storage slots are one packed list, and the party and
         * partner are INDICES into it, so a hole isn't a state the game can represent.
         * `updateBoxBlock()` already compacts on write — doing it in memory too keeps what the
         * editor shows equal to what will actually be saved, instead of showing a gap that
         * silently closes on reload.
         *
         * Every other game stores boxes POSITIONALLY and writes an explicit blank for an empty
         * slot, so for them a vacated slot staying put is already correct. **This default must stay
         * a no-op** — re-packing a positional game would shuffle Pokemon the player arranged on
         * purpose. (The roadmap used to list Legends: Arceus here too; that was wrong. PA8 writes
         * box slots at `(box*slots + slot) * stride` with a blank record for empties, exactly like
         * Sword/Shield. "Packed" in the LA code means a byte STRIDE with no padding between
         * records, which is a different thing from a gapless occupancy list.)
         *
         * @return true if any Pokemon actually changed slot, so the caller can drop cursor state
         *         (e.g. a multi-selection) that referenced slots by index.
         */
        virtual bool compactStorage() { return false; }

        /**
         * Whether this game actually STORES box names in its save.
         *
         * Let's Go does not — PKHeX's `SAV7b` implements no `IBoxDetailName` and Beluga has no
         * BoxLayout at all, because its storage is one flat 1000-slot list with no per-box
         * metadata. The `"Box N"` strings PKSE shows for LGPE are its own UI labels, so a rename
         * there would have nowhere to go. The rename UI must gate on this rather than appear and
         * silently discard the user's input.
         */
        virtual bool supportsBoxNames() const noexcept { return false; }

        /**
         * Serialize `boxNames` back into the save. Mirrors `updateBoxBlock()` and must be called
         * from the same place in each game's save function — critically, BEFORE that game's
         * checksum/hash pass, since the names live inside the checksummed region.
         *
         * Default is a no-op for the games that have nowhere to put them (see supportsBoxNames).
         */
        virtual void updateBoxNameBlock() {}

        /**
         * Serialize `currentBox` back into the save (the "U32 Box Index" block). Called from the
         * same place as updateBoxNameBlock, before the checksum pass. Default no-op for games PKSE
         * doesn't persist a current-box index for (their in-game box selection is left untouched).
         */
        virtual void updateCurrentBoxBlock() {}

        /** Longest box name this game accepts, in characters (not bytes). 0 = renaming unsupported. */
        virtual size_t getMaxBoxNameLength() const noexcept { return 0; }

        /**
         * Boxes whose name the user actually changed this session.
         *
         * `updateBoxNameBlock()` writes ONLY these, and that restriction is load-bearing. Every
         * game's box-name parser substitutes a display default ("Box 3") when the save holds an
         * empty name, so `boxNames` is a mix of real names and placeholders. Writing the whole
         * array back persists placeholders the player never typed — the round-trip harness caught
         * exactly that, 215 bytes of invented names on an untouched Z-A save.
         */
        std::vector<bool> boxNameDirty;
        void markBoxNameDirty(size_t box) {
            if (boxNameDirty.size() < boxNames.size()) boxNameDirty.resize(boxNames.size(), false);
            if (box < boxNameDirty.size()) boxNameDirty[box] = true;
        }
        bool isBoxNameDirty(size_t box) const noexcept {
            return box < boxNameDirty.size() && boxNameDirty[box];
        }

        /**
         * Whether this game's character set can represent `name`.
         *
         * The Switch keyboard happily produces accents, CJK and emoji. Gen 8/9 store UTF-16 and take
         * essentially anything, but Gen 3 has a ~70-glyph table — so a perfectly ordinary-looking
         * name can be unstorable there. The UI must ask BEFORE accepting, rather than let the write
         * path silently drop characters and hand back a mangled name.
         */
        virtual bool canStoreBoxName(const std::string& name) const { (void)name; return true; }

    protected:
        /**
         * Default constructor for derived classes.
         * Protected to prevent direct instantiation of base class.
         */
        Trainer() = default;
    };
}

#endif
