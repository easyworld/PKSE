#ifndef UI_TRAINER_VIEW_SCREEN_H
#define UI_TRAINER_VIEW_SCREEN_H

#include <cstddef>   // std::byte (details.editSnapshot)
#include <memory>
#include <string>
#include <vector>

#include <switch.h>

#include "Globals.h"          // g_injectToGameSave gates the save-destination picker
#include "UI/UIScreen.h"
#include "UI/PKSEFramebuffer.h"
#include "Trainer/Bank.h"
#include "Panels/PartyPokemonPanel.h"
#include "Panels/ItemsPanel.h"
#include "Dialogs/ItemEditDialog.h"
#include "Dialogs/SaveConfirmDialog.h"
#include "Dialogs/StatEditDialog.h"
#include "Dialogs/PickerDialog.h"
#include "Modals/PokemonDetailsModal.h"

// Forward declaration
namespace Trainer {
    class Trainer;
}

namespace UI {
    class TrainerViewScreen : public UIScreen {
    public:
        enum class ViewMode {
            Party,
            Boxes,
            Items,
            Storage,  // PKSM-style dual-pane: save boxes (left) <-> bank (right)
            Trainer,  // trainer info card (reached from the HOME main menu)
            Settings  // settings screen (auto-backup + theme)
        };

        // Cursor modes for the Storage view (cycled with Y). Colors: red / blue / green.
        enum class CursorMode {
            Menu,   // red:   A opens a per-Pokemon menu (Move / Edit / Release)
            Move,   // blue:  A directly picks up / places / swaps
            Multi   // green: A toggles multi-selection; act on the group via the group menu
        };
        // A storage slot address (which pane, box, slot).
        struct SlotRef { int pane; int box; int slot; };
        // Where the details editor's target Pokemon lives.
        enum class EditSource { Party, Box, Bank };

        TrainerViewScreen(Trainer::Trainer& trainer, const std::string& titleName, const std::string& backupDir, u64 titleId, AccountUid userUid, bool loadedFromCart);
        void update(const PadState& pad, const TouchInput& touch) override;
        void draw(PKSEFramebuffer& fb) override;
        bool shouldExit() const override { return goBack; }
        bool hasRequestedExit() const { return exitRequested; }

        // Storage (bank) view input + helpers (Phase 3.3b). Called from update().
        void handleStorageInput(u64 kDown);
        void returnHeldToOrigin();
        std::unique_ptr<Pokemon::Pokemon>& storageSlot(int pane, int box, int slot);  // pane 0=save,1=bank
        bool storageSlotLocked(int pane, int box, int slot);   // LGPE party members (save pane) are locked
        bool prepareHeldForPane(int pane);  // convert the carried mon into the save's format if needed (Phase B)
        bool convertForPane(std::unique_ptr<Pokemon::Pokemon>& pk, int destPane);  // convert a mon in place for a dest pane (Phase B)
        void buildAbilityPickerOrder(uint16_t species, uint8_t form, uint16_t current);  // legal abilities first (green + top)
        void buildCreatorSpeciesOrder();  // creator: filter the species picker to the open game's dex
        void buildMovePickerOrder(uint16_t species, uint8_t form, Enums::GameVersion group, uint16_t current);  // learnable moves first
        void openStorageEditor(int pane, int box, int slot);   // open the details modal on a storage slot
        void transferSelectionToOtherPane();                   // green group menu: bulk move the selection to the other pane
        void moveSelectionTo(int destPane, int destBox, int destSlot);  // green: drop the selection starting at a slot
        bool lgpeConversionInvolved(int destPane, const Pokemon::Pokemon* pk) const;  // would placing pk into destPane run an LGPE (AV/EV-reset) conversion?
        bool selectionInvolvesLgpe(int destPane) const;        // does any multi-selected mon trigger an LGPE conversion into destPane?
        bool gen3DowngradeInvolved(int destPane, const Pokemon::Pokemon* pk) const;  // would placing pk into destPane convert it DOWN into Gen 3 (destructive PID rebuild)?
        bool selectionInvolvesGen3Downgrade(int destPane) const;  // does any multi-selected mon trigger a Gen 3 downgrade into destPane?
        Pokemon::Pokemon* detailsTargetPokemon();              // resolve the editor's current target (party/box/bank)
        void mirrorEditedPartyMember();                        // after an edit, keep an LGPE party member's box/party copies in sync
        void snapshotEditTarget();     // capture the target's bytes as the save baseline (modal open + X = Save); revert restores to it
        bool pokemonEditDirty();       // has the details target changed vs the snapshot? (drives the "未保存的更改" marker)
        void restoreEditTarget();      // roll the details target back to the snapshot -- discards edits when the page closes without Save
        void closeDetailsModal();      // reset all details-modal / edit state and close it
        std::vector<int> visibleItemIndices() const;           // raw indices in the current pouch worth showing (count > 0)
        int currentPouchCapacity() const;                      // slot limit of the current pouch; 0 = appending unsupported
        void sortStorageBox(int pane, int box);                // pack + order one box, pinning party-linked slots
        int currentItemMaxCount() const;                       // per-stack ceiling for the current pouch (never 0)

        // Public state - accessible by UI components (Panels, Dialogs, Modals)
        Trainer::Trainer& trainer;
        std::string titleName;
        std::string backupDir;
        std::string gameVersion;  // Actual game version from NACP (e.g., "1.0.1", "1.3.2")
        u64 titleId;
        AccountUid userUid;
        bool goBack = false;
        bool exitRequested = false;  // True when user presses + to close app

        // This block is public + mutable BY DESIGN: the panels/dialogs/modals read and write it
        // directly (immediate-mode UI). The biggest cohesive clusters are grouped into nested structs
        // (statEdit / creator / details); the rest are flat single-purpose flags + indices, each
        // default-initialised in place so the constructor only wires up the ctor arguments.

        ViewMode selectedMode = ViewMode::Party;
        int currentPage = 0;
        int selectedCategory = 0;  // For Items mode: selected PouchType
        int selectedBoxIndex = 0;  // For Boxes mode: selected box (0-31)
        int selectedPartyIndex = 0;  // For Party mode: selected party Pokemon (0-5)
        bool detailViewActive = false;  // True when detail panel is active (for Items/Boxes)
        int selectedItemIndex = 0;  // Selected item/pokemon index in detail view (item for Items, slot for Boxes)

        // HOME main menu focus (shown when NOT entered). 0 Pokemon(Boxes), 1 Party, 2 Storage (pills);
        // 3 Items, 4 Trainer, 5 Settings (circular icons). Replaces the old left mode-selector.
        int homeMenuIndex = 0;

        // Box cursor scale-pop (Phase 4.6): the selected disc briefly pops when the cursor moves.
        int animPrevBoxSlot = -1;
        double animBoxPopStart = -100.0;

        // Selected row in the Settings view (0-4); reached from the menu's Settings icon.
        int settingsSelectedRow = 0;

        // Box swap (Phase 3 3.1): press Y to grab the slot under the cursor, then Y on another
        // occupied slot to swap them. swapSourceBox/Slot record the grabbed slot.
        bool swapActive = false;
        int swapSourceBox = -1;
        int swapSourceSlot = -1;

        // A tap on a box-name pill sets this instead of opening the rename immediately, so the header
        // highlight draws one frame BEFORE the blocking keyboard applet (otherwise the selection only
        // appears after the dialog closes). Consumed at the top of the next update().
        bool pendingHeaderRename = false;

        // --- PKSM-style Storage view: dual-pane save boxes <-> bank ---
        std::unique_ptr<Trainer::Bank> bank;        // created in the ctor
        CursorMode cursorMode = CursorMode::Menu;   // default red (menu); cycled with Y
        int storageFocusPane = 0;                   // 0 = save (left), 1 = bank (right)
        int stSaveBox = 0, stSaveSlot = 0;          // cursor in the save pane
        int stBankBox = 0, stBankSlot = 0;          // cursor in the bank pane
        std::string storageStatus;                  // transient status line (e.g. a refused cross-game drop)
        int storageStatusFrames = 0;                // frames left to show storageStatus (counts down)
        // Post a message to that line. This is PKSE's only way to tell the user anything: there is no
        // console on the Switch, and logErrorToFile writes somewhere they cannot read while the app is
        // running. Anything a user needs to know about MUST come through here, not just the log.
        void postStatus(const std::string& msg, int frames = 240) {
            storageStatus = msg;
            storageStatusFrames = frames;
        }
        // Carried single Pokemon (Move-mode pick-up, or the red menu's "招式").
        std::unique_ptr<Pokemon::Pokemon> heldPokemon;
        int heldPane = 0, heldFromBox = 0, heldFromSlot = 0;   // origin (for cancel/return)
        // Red per-Pokemon action menu (Move / Edit / Release / Cancel).
        bool storageMenuActive = false;
        int storageMenuIndex = 0;
        int menuPane = 0, menuBox = 0, menuSlot = 0;           // the slot the menu acts on
        // Green multi-selection + its group menu (Move / Release / Clear / Cancel).
        std::vector<SlotRef> multiSel;
        bool groupMenuActive = false;
        int groupMenuIndex = 0;
        // Release confirmation (single slot, or the whole multi-selection when releaseGroup).
        bool releaseConfirmActive = false;
        int releasePane = 0, releaseBox = 0, releaseSlot = 0;
        bool releaseGroup = false;
        // Leaving the storage view with unsaved bank changes: prompt Save / Discard / Cancel (PKSM-style).
        bool storageExitConfirmActive = false;
        int storageExitConfirmIndex = 0;   // 0=Save & Exit, 1=Discard & Exit, 2=Cancel
        // Set when + (exit app) raised that prompt instead of B. Closing the app is not an answer to
        // "save the bank?", so + asks first and the exit resumes once Save or Discard has been picked.
        bool exitAfterBankChoice = false;
        void beginAppExit();   // the + handler's tail: prompt about the game save, else leave
        // Touchable storage slot rects, captured during draw for tap hit-testing next frame.
        struct TouchTarget { int pane, box, slot, x, y, w, h; };
        std::vector<TouchTarget> storageTouchTargets;
        // Touchable popup/dialog buttons (id = menu item index, or a dialog-specific id), captured
        // during draw and hit-tested next frame. Only the active overlay populates this.
        struct TouchButton { int id, x, y, w, h; };
        std::vector<TouchButton> touchButtons;
        int touchedButtonId(const TouchInput& touch) const;  // id of a tapped button, or -1
        void renameBox(int boxIndex);       // swkbd rename of a SAVE box; no-op where unsupported
        void renameBankBox(int box);        // swkbd rename of a BANK box (default label is "银行箱 N")

        // Item editing state
        bool itemEditDialogActive = false;  // True when editing an item's amount
        int itemEditDialogValue = 0;    // Current value being edited
        int itemEditDialogOriginalValue = 0;  // Original value before editing
        // Items list: Y asks before removing the selected item. A in this dialog does the delete,
        // B cancels. Only reachable from the Items view, so it never lets X-to-save fire (home-menu only).
        bool itemRemoveConfirmActive = false;

        // Save confirmation state
        bool saveConfirmActive = false;
        // Save destination picker, on PKSM's model.
        //   0 = this backup   1 = new named backup   2 = game save
        //
        // Whether "游戏存档" is offered depends on WHERE THIS SESSION CAME FROM, not on a blanket
        // setting:
        //   - Loaded from the live save  -> always offered, and the default. Writing your own save
        //     back is the ordinary thing a save editor does; gating it behind a toggle is friction
        //     for no safety gain, because the data you'd overwrite is the data you just read.
        //   - Loaded from an older backup -> offered only when the Settings lock is on, and it
        //     raises an extra confirmation, because THIS is the case that rolls a game backwards.
        enum SaveDest { DestThisBackup = 0, DestNewBackup = 1, DestGameSave = 2 };
        bool loadedFromCart = false;
        int saveDestIndex = DestThisBackup;
        int saveDestCount() const { return (loadedFromCart || g_injectToGameSave) ? 3 : 2; }
        /// The default destination when the save dialog opens.
        int defaultSaveDest() const { return loadedFromCart ? DestGameSave : DestThisBackup; }

        /// Set once a value outside the games' own limits has actually been committed this session
        /// (EV > 252, AV > 200, or an EV total over 510 — only reachable with "允许非法数值"
        /// on). The save dialog then carries a warning line. Entirely separate from the destination
        /// logic: it is about what is being written, not where.
        bool illegalDataWritten = false;
        // Create sdmc:/PKSE/{title}/{name}/ seeded with a copy of the current backup, suffixing
        // -2, -3... if the name is taken. Returns the new path, or "" on failure.
        std::string createNamedBackupDir(const std::string& name);
        // Last gate before overwriting the player's real save data. Reached only by choosing the
        // "游戏存档" destination, which is itself only offered when the Settings lock is on.
        bool saveInjectConfirmActive = false;
        // Write to destDir, optionally injecting into the game. Shared by every destination so the
        // success/failure handling can't drift between them.
        void performSave(const std::string& destDir, bool injectToTitle);
        bool hasUnsavedChanges = false;
        bool exitingWithUnsavedChanges = false;
        bool exitingViaPlus = false;  // True when exiting via + button (exit app) vs B button (go back)

        // Stat editor (IV / EV / AV + shiny). Original* is the value on dialog entry; Current* is the
        // in-progress edit, preserved when switching between the IV/EV/AV modes.
        struct StatEditState {
            bool dialogActive = false;
            int  selectedStat = 0;                          // 0-5: HP, ATK, DEF, SPE, SPA, SPD
            Dialogs::StatEditMode mode = Dialogs::StatEditMode::IV;
            int  value = 0;                                 // current edit (the active IV/EV/AV)
            int  originalIV = 0, originalEV = 0, originalAV = 0;   // on entry (AV = Let's Go)
            int  currentIV = 0,  currentEV = 0,  currentAV = 0;    // edits, kept across mode switches
        };
        StatEditState statEdit;

        // Pokemon details modal (the HOME "查看能力" editor page): open/target, cursor, overlays,
        // hexagon mode, the left-pane scroll + nav list, and the edit baseline that drives the top-bar
        // "未保存的更改" marker (snapshot on open, re-taken by X = Save; empty when not editing).
        struct DetailsState {
            bool active = false;
            EditSource source = EditSource::Box;         // where the edited Pokemon lives
            int  bankBox = 0, bankSlot = 0;              // bank target (EditSource::Bank)
            int  partyIndex = 0;                         // party slot (0-5) when editing a party mon
            int  category = 0;                           // 0=Main,1=Met,2=Stats,3=Moves,4=Cosmetic,5=OT/Misc
            int  selectedStat = 0;                       // Stats category: which stat is selected
            int  selectedField = 0;                      // center column: selected field (see the modal draw)
            bool editing = false;                        // editing a stat value / main field
            int  hexMode = 0;                            // hexagon (Y-cycled): 0 Summary, 1 Base Points (EV/AV), 2 Judge (IVs)
            bool legalityOverlay = false;                // full legality issue list (Y / tap)
            bool ribbonOverlay = false;                  // full ribbon/mark list (X / tap)
            int  lastCenterField = 0;                    // remembered center-column row when hopping to/from moves
            int  leftScroll = 0;                         // left-pane vertical scroll (px); auto-follows selection, reset on (re)open
            std::vector<int> leftOrder;                  // left-pane editable field ids in DRAW order (rebuilt each draw)
            std::vector<std::byte> editSnapshot;         // baseline bytes for the "未保存的更改" marker + revert
        };
        DetailsState details;
        // Creator: the Species picker was opened in "新建宝可梦" mode; where the built mon lands,
        // plus the not-yet-accepted-mon editing flow (Keep/Discard on exit).
        struct CreatorState {
            bool active = false;                 // Species picker is in create-a-new-mon mode
            int  pane = 0, box = 0, slot = 0;    // where the built mon is dropped
            bool editing = false;                // details modal is on a just-created, not-yet-accepted mon
            bool keepConfirmActive = false;      // "保留这只新宝可梦吗？" prompt shown on exit
            int  keepConfirmIndex = 1;           // cursor: 0 = Discard, 1 = Keep (default)
        };
        CreatorState creator;
        // Let's Go move acknowledgement (settings-gated by g_lgpeMoveWarn): moving a mon to/from LGPE
        // resets AVs/EVs, so the user confirms first. The pending storage action is stashed + run on Yes.
        enum class LgpePending { None, PlaceHeld, GroupMoveTo, GroupTransfer };
        bool lgpeMoveConfirmActive = false;
        int  lgpeMoveConfirmIndex = 1;          // cursor: 0 = Cancel, 1 = Continue (default)
        LgpePending lgpePending = LgpePending::None;
        bool moveConfirmGen3 = false;           // pending confirm is a Gen 3 downgrade (PID rebuild), not only an LGPE AV/EV reset
        int lgpePendPane = 0, lgpePendBox = 0, lgpePendSlot = 0;

        // Reusable selection panel (picker) for choosing a value from a list — nature, gender, move.
        bool pickerActive = false;
        Dialogs::PickerKind pickerKind = Dialogs::PickerKind::Nature;
        int pickerSlot = 0;   // which of the 4 move slots (when pickerKind == Move)
        int pickerSel = 0;    // highlighted option index
        int pickerCount = 0;  // number of options in the list
        // A pouch-item picker opened to CHANGE an existing item's type (Potion -> Super Potion),
        // not to add a new one. Changes the confirm behavior + the picker title; reuses PouchItem.
        bool itemPickerReplace = false;
        // Ability picker: reordered option list so the species' legal abilities sort to the top
        // and render green. pickerOrder[row] = ability id at that row; rows 0..pickerLegalCount-1 are
        // the legal abilities. Empty for every other picker kind (which stay identity-indexed: row == value).
        std::vector<int> pickerOrder;
        int pickerLegalCount = 0;
        // Met-location picker: the origin version whose location table is shown, so the picker can
        // resolve each id in pickerOrder to a name (a location id names a different place per game).
        uint8_t pickerMetVersion = 0;
        // Form picker: the species whose forms are being listed, so the picker can name each form.
        uint16_t pickerFormSpecies = 0;
        // Location picker mode: true routes the pick to the egg-met location, false to the met location.
        bool pickerMetIsEgg = false;
    };
}

#endif
