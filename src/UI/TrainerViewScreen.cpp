#include <cstring>
#include <algorithm>

#include "Globals.h"
#include "Save/GetSaveFileContents.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/Panels/TrainerInfoPanel.h"
#include "UI/Panels/ModeSelectorPanel.h"
#include "UI/Panels/PartyPokemonPanel.h"
#include "UI/Panels/BoxPokemonPanel.h"
#include "UI/Panels/ItemsPanel.h"
#include "UI/Dialogs/ItemEditDialog.h"
#include "UI/Dialogs/SaveConfirmDialog.h"
#include "UI/Dialogs/StatEditDialog.h"
#include "UI/Modals/PokemonDetailsModal.h"
#include "Utils/HelperUtilities.h"
#include "Utils/Logger.h"
#include "Utils/FileUtilities.h"
#include "Trainer/Trainer.h"
#include "Trainer/Inventory.h"
#include "Trainer/Inventory9LZA.h"
#include "Pokemon/Pokemon.h"

namespace UI {
    // UI Layout constants
    constexpr int LEFT_PANEL_X = 0;
    constexpr int LEFT_PANEL_Y = 70;
    constexpr int LEFT_TRAINER_INFO_PANEL_WIDTH = 200;
    constexpr int LEFT_TRAINER_INFO_PANEL_HEIGHT = 150;
    constexpr int LEFT_VIEW_MODE_PANEL_WIDTH = 200;
    constexpr int LEFT_VIEW_MODE_PANEL_HEIGHT = 150;
    constexpr int CONTENT_PANEL_X = LEFT_PANEL_X + LEFT_VIEW_MODE_PANEL_WIDTH + 10;
    constexpr int CONTENT_PANEL_Y = LEFT_PANEL_Y;
    constexpr int CONTENT_PANEL_HEIGHT = 575;

    TrainerViewScreen::TrainerViewScreen(Trainer::Trainer& trainer, const std::string& titleName, const std::string& backupDir, u64 titleId, AccountUid userUid)
        : trainer(trainer), titleName(titleName), backupDir(backupDir), titleId(titleId), userUid(userUid), scrollOffset(0), goBack(false), exitRequested(false),
        selectedMode(ViewMode::Party), currentPage(0), totalPages(1), selectedCategory(0), selectedBoxIndex(0), selectedPartyIndex(0),
        detailViewActive(false), selectedItemIndex(0), itemEditDialogActive(false), itemEditDialogValue(0),
        itemEditDialogOriginalValue(0), saveConfirmActive(false), hasUnsavedChanges(false), exitingWithUnsavedChanges(false), exitingViaPlus(false),
        statEditDialogActive(false), statEditSelectedStat(0), statEditMode(Dialogs::StatEditMode::IV),
        statEditValue(0), statEditOriginalIV(0), statEditOriginalEV(0), statEditCurrentIV(0), statEditCurrentEV(0),
        statEditOriginalShiny(false), statEditCurrentShiny(false),
        pokemonDetailsActive(false), pokemonDetailsIsParty(false), pokemonDetailsPartyIndex(0),
        pokemonDetailsCategory(0), pokemonDetailsSelectedStat(0), pokemonDetailsSelectedField(0),
        pokemonDetailsEditing(false), pokemonDetailsEditValue(0) {
    }

    void TrainerViewScreen::update(const PadState& pad) {
        u64 kDown = padGetButtonsDown(&pad);

        // Handle + button (exits application)
        if (kDown & HidNpadButton_Plus) {
            // Check for unsaved changes
            if (hasUnsavedChanges && !saveConfirmActive) {
                // Prompt to save changes before exiting
                exitingWithUnsavedChanges = true;
                exitingViaPlus = true;  // Remember we're exiting via + button
                saveConfirmActive = true;
                return;
            }
            // No unsaved changes or already handled, exit immediately
            exitRequested = true;
            return;
        }

        // Handle X button (save confirmation)
        if (kDown & HidNpadButton_X) {
            if (!saveConfirmActive && !itemEditDialogActive && !statEditDialogActive) {
                exitingWithUnsavedChanges = false;  // Regular save, not exiting
                exitingViaPlus = false;
                saveConfirmActive = true;
                return;
            }
        }

        // Handle save confirmation dialog
        if (saveConfirmActive) {
            if (exitingWithUnsavedChanges) {
                // Exiting with unsaved changes - different button logic
                // A: Discard changes and exit
                // B: Cancel exit (stay on screen)
                if (kDown & HidNpadButton_A) {
                    // User chose to discard changes and exit
                    saveConfirmActive = false;
                    if (exitingViaPlus) {
                        // Exiting via + button - exit the app
                        exitRequested = true;
                    }
                    // Always set goBack for consistency (B button case)
                    goBack = true;
                    exitingWithUnsavedChanges = false;
                    exitingViaPlus = false;
                    return;
                }
                if (kDown & HidNpadButton_B) {
                    // User cancelled exit - stay on screen
                    saveConfirmActive = false;
                    exitingWithUnsavedChanges = false;
                    exitingViaPlus = false;
                    return;
                }
            } else {
                // Regular save dialog (triggered by X button)
                // A: Save changes
                // B: Cancel
                if (kDown & HidNpadButton_A) {
                    // User confirmed save - auto-detects game version and uses appropriate save function
                    bool saveSuccess = Save::saveTrainerInfo(trainer, backupDir.c_str(), titleId, userUid);
                    saveConfirmActive = false;

                    if (saveSuccess) {
                        hasUnsavedChanges = false;
                        // Close dialogs/modals if there are any open and return to the View Mode selection state
                        statEditDialogActive = false;
                        pokemonDetailsActive = false;
                        pokemonDetailsEditing = false;
                    }
                    // If save failed, stay on current screen (error logged by save function)
                    return;
                }
                if (kDown & HidNpadButton_B) {
                    // User cancelled save - just close the dialog
                    saveConfirmActive = false;
                    return;
                }
            }
            return;  // Don't process other inputs while save confirm is active
        }

        // Handle stat edit dialog
        if (statEditDialogActive) {
            // Get the Pokemon being edited based on whether it's party or box Pokemon
            Pokemon::Pokemon* pokemon = nullptr;
            if (pokemonDetailsIsParty) {
                // Editing party Pokemon
                if (pokemonDetailsPartyIndex >= 0 && pokemonDetailsPartyIndex < static_cast<int>(trainer.party.size())) {
                    pokemon = trainer.party[pokemonDetailsPartyIndex].get();
                }
            } else {
                // Editing box Pokemon
                if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                    selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                    auto& boxPokemon = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                    if (boxPokemon) {
                        pokemon = boxPokemon.get();
                    }
                }
            }

            if (pokemon) {
                    // Up/Down to switch between IV and EV
                    if (kDown & HidNpadButton_Up) {
                        // Save current value before switching
                        if (statEditMode == Dialogs::StatEditMode::EV) {
                            statEditCurrentEV = statEditValue;
                        }
                        statEditMode = Dialogs::StatEditMode::IV;
                        statEditValue = statEditCurrentIV;  // Load IV value
                    }
                    if (kDown & HidNpadButton_Down) {
                        // Save current value before switching
                        if (statEditMode == Dialogs::StatEditMode::IV) {
                            statEditCurrentIV = statEditValue;
                        }
                        statEditMode = Dialogs::StatEditMode::EV;
                        statEditValue = statEditCurrentEV;  // Load EV value
                    }

                    // Get max value based on mode
                    int minValue = 0;
                    int maxValue = (statEditMode == Dialogs::StatEditMode::IV) ? 31 : 252;

                    // Adjust value with different increments
                    if (kDown & HidNpadButton_Left) {
                        statEditValue = std::max(minValue, statEditValue - 1);
                    }
                    if (kDown & HidNpadButton_Right) {
                        statEditValue = std::min(maxValue, statEditValue + 1);
                    }
                    if (kDown & HidNpadButton_R) {
                        statEditValue = std::min(maxValue, statEditValue + 10);
                    }
                    if (kDown & HidNpadButton_L) {
                        statEditValue = std::max(minValue, statEditValue - 10);
                    }

                    // Update the current value for the active mode
                    if (statEditMode == Dialogs::StatEditMode::IV) {
                        statEditCurrentIV = statEditValue;
                    } else {
                        statEditCurrentEV = statEditValue;
                    }

                    // ZL/ZR for EVs only
                    if (statEditMode == Dialogs::StatEditMode::EV) {
                        if (kDown & HidNpadButton_ZL) {
                            statEditValue = std::max(minValue, statEditValue - 100);
                        }
                        if (kDown & HidNpadButton_ZR) {
                            statEditValue = std::min(maxValue, statEditValue + 100);
                        }
                    }

                    // For EVs, check total constraint
                    if (statEditMode == Dialogs::StatEditMode::EV) {
                        int totalEVs = pokemon->evHP() + pokemon->evATK() + pokemon->evDEF() +
                            pokemon->evSPE() + pokemon->evSPA() + pokemon->evSPD();
                        int projectedTotal = totalEVs - statEditOriginalEV + statEditValue;
                        if (projectedTotal > 510) {
                            // Adjust value to not exceed total
                            statEditValue = std::max(0, 510 - (totalEVs - statEditOriginalEV));
                        }
                    }

                    // Confirm edit
                    if (kDown & HidNpadButton_A) {
                        // Map UI stat index to Pokemon data stat index
                        // UI order: HP, ATK, DEF, SPA, SPD, SPE (indices 0-5)
                        // Data order: HP, ATK, DEF, SPE, SPA, SPD (indices 0-5)
                        int statIndexMap[] = {0, 1, 2, 4, 5, 3}; // UI index -> Data index
                        int dataStatIndex = statIndexMap[statEditSelectedStat];

                        // Apply IV and EV changes if they were modified
                        bool statsModified = false;
                        if (statEditCurrentIV != statEditOriginalIV) {
                            pokemon->setIV(dataStatIndex, statEditCurrentIV);
                            hasUnsavedChanges = true;
                            statsModified = true;
                        }
                        if (statEditCurrentEV != statEditOriginalEV) {
                            pokemon->setEV(dataStatIndex, statEditCurrentEV);
                            hasUnsavedChanges = true;
                            statsModified = true;
                        }

                        // Regenerate PID to maintain legality after IV/EV changes
                        if (statsModified) {
                            // pokemon->regeneratePID(trainer.ID32);
                            pokemon->regeneratePID(pokemon->id32());
                        }

                        statEditDialogActive = false;
                        return;
                    }

                    // Cancel edit
                    if (kDown & HidNpadButton_B) {
                        statEditDialogActive = false;
                        return;
                    }
            }

            return;  // Don't process other inputs while stat edit dialog is active
        }

        // Handle Pokemon details modal
        if (pokemonDetailsActive) {
            if (kDown & HidNpadButton_B) {
                if (pokemonDetailsEditing) {
                    // If editing, go back to category selection
                    pokemonDetailsEditing = false;
                } else {
                    // Close modal
                    pokemonDetailsActive = false;
                    pokemonDetailsIsParty = false;
                    pokemonDetailsEditing = false;
                    pokemonDetailsCategory = 0;
                    pokemonDetailsSelectedStat = 0;
                }
                return;
            }

            // If not editing, allow category navigation
            if (!pokemonDetailsEditing) {
                if (kDown & HidNpadButton_Up) {
                    pokemonDetailsCategory = (pokemonDetailsCategory - 1 + 6) % 6;
                }
                if (kDown & HidNpadButton_Down) {
                    pokemonDetailsCategory = (pokemonDetailsCategory + 1) % 6;
                }

                // In Main category, A starts editing
                if (pokemonDetailsCategory == 0 && (kDown & HidNpadButton_A)) {
                    pokemonDetailsEditing = true;
                    pokemonDetailsSelectedField = 0;
                }

                // In Stats category, A starts editing
                if (pokemonDetailsCategory == 2 && (kDown & HidNpadButton_A)) {
                    pokemonDetailsEditing = true;
                    pokemonDetailsSelectedStat = 0;
                }
            } else if (pokemonDetailsCategory == 0) {
                // Editing main fields
                // Up/Down to select field
                int fields = 15; // TODO: We need to make this more dynamic and eventually will want to make all fields editable
                if (kDown & HidNpadButton_Up) {
                    pokemonDetailsSelectedField = (pokemonDetailsSelectedField - 1 + fields) % fields;  // number of fields in Main
                }
                if (kDown & HidNpadButton_Down) {
                    pokemonDetailsSelectedField = (pokemonDetailsSelectedField + 1) % fields;
                }

                // A to edit the selected field
                if (kDown & HidNpadButton_A) {
                    // Get the Pokemon being edited
                    Pokemon::Pokemon* pokemon = nullptr;
                    if (pokemonDetailsIsParty) {
                        if (pokemonDetailsPartyIndex >= 0 && pokemonDetailsPartyIndex < static_cast<int>(trainer.party.size())) {
                            pokemon = trainer.party[pokemonDetailsPartyIndex].get();
                        }
                    } else {
                        if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                            selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                            auto& boxPokemon = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                            if (boxPokemon) {
                                pokemon = boxPokemon.get();
                            }
                        }
                    }

                    if (pokemon) {
                        // Field 4 is Shiny
                        if (pokemonDetailsSelectedField == 4) {
                            // Toggle shiny status
                            logInfoToFile("Shiny field selected, toggling...");
                            bool currentShiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
                            pokemon->setShiny(!currentShiny, pokemon->id32());
                            hasUnsavedChanges = true;
                        }
                        // TODO: Future: Handle other fields (PID, Species, Gender, Nickname, EXP, Level, Nature, Held Item, Ability)
                    } else {
                        logInfoToFile("Pokemon pointer is null!");
                    }
                }
            } else if (pokemonDetailsCategory == 2) {
                // Editing stats
                // Up/Down to select stat
                if (kDown & HidNpadButton_Up) {
                    pokemonDetailsSelectedStat = (pokemonDetailsSelectedStat - 1 + 6) % 6;
                }
                if (kDown & HidNpadButton_Down) {
                    pokemonDetailsSelectedStat = (pokemonDetailsSelectedStat + 1) % 6;
                }

                // A to open stat edit dialog
                if (kDown & HidNpadButton_A) {
                    // Get the Pokemon based on whether we're editing party or box Pokemon
                    const Pokemon::Pokemon* pokemon = nullptr;
                    if (pokemonDetailsIsParty) {
                        // Editing party Pokemon
                        if (pokemonDetailsPartyIndex >= 0 && pokemonDetailsPartyIndex < static_cast<int>(trainer.party.size())) {
                            pokemon = trainer.party[pokemonDetailsPartyIndex].get();
                        }
                    } else {
                        // Editing box Pokemon
                        if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                            selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                            const auto& boxPokemon = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                            if (boxPokemon) {
                                pokemon = boxPokemon.get();
                            }
                        }
                    }

                    if (pokemon) {
                        statEditSelectedStat = pokemonDetailsSelectedStat;

                        // Get original values for this stat
                        switch (pokemonDetailsSelectedStat) {
                            case 0: statEditOriginalIV = pokemon->ivHP(); statEditOriginalEV = pokemon->evHP(); break;
                            case 1: statEditOriginalIV = pokemon->ivATK(); statEditOriginalEV = pokemon->evATK(); break;
                            case 2: statEditOriginalIV = pokemon->ivDEF(); statEditOriginalEV = pokemon->evDEF(); break;
                            case 3: statEditOriginalIV = pokemon->ivSPA(); statEditOriginalEV = pokemon->evSPA(); break;
                            case 4: statEditOriginalIV = pokemon->ivSPD(); statEditOriginalEV = pokemon->evSPD(); break;
                            case 5: statEditOriginalIV = pokemon->ivSPE(); statEditOriginalEV = pokemon->evSPE(); break;
                        }

                        // Initialize current values to original values
                        statEditCurrentIV = statEditOriginalIV;
                        statEditCurrentEV = statEditOriginalEV;

                        // Start with IV editing mode
                        statEditMode = Dialogs::StatEditMode::IV;
                        statEditValue = statEditCurrentIV;
                        statEditDialogActive = true;
                    }
                }
            }

            return;  // Don't process other inputs while modal is active
        }

        // Handle edit dialog
        if (itemEditDialogActive) {
            // Adjust value with different increments
            if (kDown & HidNpadButton_Left) {
                itemEditDialogValue = std::max(0, itemEditDialogValue - 1);
            }
            if (kDown & HidNpadButton_Right) {
                itemEditDialogValue = std::min(999, itemEditDialogValue + 1);
            }
            if (kDown & HidNpadButton_Up) {
                itemEditDialogValue = std::min(999, itemEditDialogValue + 10);
            }
            if (kDown & HidNpadButton_Down) {
                itemEditDialogValue = std::max(0, itemEditDialogValue - 10);
            }
            if (kDown & HidNpadButton_ZL) {
                itemEditDialogValue = std::max(0, itemEditDialogValue - 100);
            }
            if (kDown & HidNpadButton_ZR) {
                itemEditDialogValue = std::min(999, itemEditDialogValue + 100);
            }

            // Confirm edit
            if (kDown & HidNpadButton_A) {
                // Save the new value to the item
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    auto& pouch = trainer.items[selectedCategory];
                    if (selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(pouch.size())) {
                        pouch[selectedItemIndex].count = itemEditDialogValue;
                        if (itemEditDialogValue != itemEditDialogOriginalValue) {
                            hasUnsavedChanges = true;
                        }
                    }
                }
                itemEditDialogActive = false;
                return;
            }

            // Cancel edit
            if (kDown & HidNpadButton_B) {
                itemEditDialogActive = false;
                return;
            }

            return;  // Don't process other inputs while edit dialog is active
        }

        // Handle detail view exit
        if (detailViewActive) {
            if (kDown & HidNpadButton_B) {
                // Exit detail view (B button only)
                detailViewActive = false;
                selectedItemIndex = 0;
                currentPage = 0;
                return;
            }

            // Handle detail view navigation
            if (selectedMode == ViewMode::Items) {
                // Get current pouch size for bounds checking
                if (selectedCategory >= 0 && selectedCategory < static_cast<int>(trainer.items.size())) {
                    const auto& pouch = trainer.items[selectedCategory];
                    // Calculate items per column based on panel height
                    int itemsPerColumn = (CONTENT_PANEL_HEIGHT - 120) / 20;  // (height - header) / lineHeight
                    int itemsPerPage = itemsPerColumn * 2;  // Two columns
                    int totalItems = pouch.size();
                    int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;

                    // Up/Down to select items
                    if (kDown & HidNpadButton_Up) {
                        selectedItemIndex = (selectedItemIndex - 1 + totalItems) % totalItems;
                        // Adjust page if needed
                        currentPage = selectedItemIndex / itemsPerPage;
                    }
                    if (kDown & HidNpadButton_Down) {
                        selectedItemIndex = (selectedItemIndex + 1) % totalItems;
                        // Adjust page if needed
                        currentPage = selectedItemIndex / itemsPerPage;
                    }

                    // Left/Right to navigate columns, then pages
                    if (kDown & HidNpadButton_Right) {
                        // Calculate current position within page
                        int indexInPage = selectedItemIndex % itemsPerPage;
                        int currentColumn = (indexInPage >= itemsPerColumn) ? 1 : 0;
                        int currentRow = indexInPage % itemsPerColumn;

                        if (currentColumn == 0) {
                            // In left column, try to move to right column
                            int newIndex = selectedItemIndex + itemsPerColumn;
                            // Check if item exists in right column of current page
                            if (newIndex < std::min((currentPage + 1) * itemsPerPage, totalItems)) {
                                selectedItemIndex = newIndex;
                            } else {
                                // No item to the right, wrap to next page if available
                                if (currentPage < totalPages - 1) {
                                    currentPage++;
                                    selectedItemIndex = currentPage * itemsPerPage + currentRow;
                                    // Make sure we don't exceed total items
                                    if (selectedItemIndex >= totalItems) {
                                        selectedItemIndex = currentPage * itemsPerPage;
                                    }
                                }
                            }
                        } else {
                            // In right column, move to next page, left column, same row
                            if (currentPage < totalPages - 1) {
                                currentPage++;
                                selectedItemIndex = currentPage * itemsPerPage + currentRow;
                                // Make sure we don't exceed total items
                                if (selectedItemIndex >= totalItems) {
                                    selectedItemIndex = currentPage * itemsPerPage;
                                }
                            }
                        }
                    }

                    if (kDown & HidNpadButton_Left) {
                        // Calculate current position within page
                        int indexInPage = selectedItemIndex % itemsPerPage;
                        int currentColumn = (indexInPage >= itemsPerColumn) ? 1 : 0;
                        int currentRow = indexInPage % itemsPerColumn;

                        if (currentColumn == 1) {
                            // In right column, move to left column
                            selectedItemIndex = selectedItemIndex - itemsPerColumn;
                        } else {
                            // In left column, move to previous page, right column, same row
                            if (currentPage > 0) {
                                currentPage--;
                                // Try to go to same row in right column of previous page
                                int targetIndex = (currentPage * itemsPerPage) + itemsPerColumn + currentRow;
                                // Make sure target exists in previous page
                                if (targetIndex < std::min((currentPage + 1) * itemsPerPage, totalItems)) {
                                    selectedItemIndex = targetIndex;
                                } else {
                                    // If right column row doesn't exist, go to last item of previous page
                                    selectedItemIndex = std::min((currentPage + 1) * itemsPerPage, totalItems) - 1;
                                }
                            }
                        }
                    }

                    // A button to edit item amount
                    if (kDown & HidNpadButton_A) {
                        if (!pouch.empty() && selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(pouch.size())) {
                            // Open edit dialog for selected item
                            itemEditDialogActive = true;
                            itemEditDialogValue = pouch[selectedItemIndex].count;
                            itemEditDialogOriginalValue = pouch[selectedItemIndex].count;
                        }
                    }

                    // L/R to change categories (but not when in edit dialog)
                    if (kDown & HidNpadButton_L) {
                        switch(trainer.getGameGroup()) {
                            case GameVersion::ZA: {
                                selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_LZA) % POUCH_COUNT9_LZA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SWSH: {
                                selectedCategory = (selectedCategory - 1 + 9) % 9; // TODO: Need to add global variable for this value
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            default: break;
                        }
                    }
                    if (kDown & HidNpadButton_R) {
                        switch(trainer.getGameGroup()) {
                            case GameVersion::ZA: {
                                selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_LZA;
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            case GameVersion::SWSH: {
                                selectedCategory = (selectedCategory + 1) % 9; // TODO: Need to add global variable for this value
                                currentPage = 0;
                                selectedItemIndex = 0;
                                break;
                            }
                            default: break;
                        }
                    }
                }
            }

            // Handle detail view navigation for Boxes mode
            if (selectedMode == ViewMode::Boxes) {
                constexpr int GRID_COLS = 6;
                constexpr int GRID_ROWS = 5;

                // Up/Down/Left/Right to navigate grid
                if (kDown & HidNpadButton_Up) {
                    int currentRow = selectedItemIndex / GRID_COLS;
                    int currentCol = selectedItemIndex % GRID_COLS;
                    if (currentRow > 0) {
                        selectedItemIndex -= GRID_COLS;
                    } else {
                        // Wrap to bottom row, same column
                        selectedItemIndex = (GRID_ROWS - 1) * GRID_COLS + currentCol;
                    }
                }
                if (kDown & HidNpadButton_Down) {
                    int currentRow = selectedItemIndex / GRID_COLS;
                    int currentCol = selectedItemIndex % GRID_COLS;
                    if (currentRow < GRID_ROWS - 1) {
                        selectedItemIndex += GRID_COLS;
                    } else {
                        // Wrap to top row, same column
                        selectedItemIndex = currentCol;
                    }
                }
                if (kDown & HidNpadButton_Left) {
                    if (selectedItemIndex % GRID_COLS > 0) {
                        selectedItemIndex--;
                    } else {
                        // Wrap to end of row
                        int currentRow = selectedItemIndex / GRID_COLS;
                        selectedItemIndex = currentRow * GRID_COLS + (GRID_COLS - 1);
                    }
                }
                if (kDown & HidNpadButton_Right) {
                    if (selectedItemIndex % GRID_COLS < GRID_COLS - 1) {
                        selectedItemIndex++;
                    } else {
                        // Wrap to start of row
                        int currentRow = selectedItemIndex / GRID_COLS;
                        selectedItemIndex = currentRow * GRID_COLS;
                    }
                }

                // L/R to change boxes
                if (kDown & HidNpadButton_L) {
                    selectedBoxIndex = (selectedBoxIndex - 1 + 32) % 32;  // 32 boxes
                }
                if (kDown & HidNpadButton_R) {
                    selectedBoxIndex = (selectedBoxIndex + 1) % 32;  // 32 boxes
                }

                // A button to view details
                if (kDown & HidNpadButton_A) {
                    // Only open if there's a pokemon in the selected slot
                    if (selectedBoxIndex >= 0 && selectedBoxIndex < static_cast<int>(trainer.boxes.size()) &&
                        selectedItemIndex >= 0 && selectedItemIndex < static_cast<int>(BOX_SLOTS)) {
                        const auto& pokemon = trainer.boxes[selectedBoxIndex][selectedItemIndex];
                        if (pokemon) {
                            pokemonDetailsActive = true;
                            pokemonDetailsIsParty = false;
                            pokemonDetailsCategory = 0;
                            pokemonDetailsSelectedStat = 0;
                            pokemonDetailsEditing = false;
                        }
                    }
                }
            }

            // Handle detail view navigation for Party mode
            if (selectedMode == ViewMode::Party) {
                constexpr int COLUMN_SIZE = 3;

                // Determine current column (0 = left, 1 = right)
                int currentColumn = (selectedPartyIndex >= COLUMN_SIZE) ? 1 : 0;
                int rowInColumn = selectedPartyIndex % COLUMN_SIZE;

                // Up/Down to navigate within current column
                if (kDown & HidNpadButton_Up) {
                    rowInColumn = (rowInColumn - 1 + COLUMN_SIZE) % COLUMN_SIZE;
                    selectedPartyIndex = currentColumn * COLUMN_SIZE + rowInColumn;
                }
                if (kDown & HidNpadButton_Down) {
                    rowInColumn = (rowInColumn + 1) % COLUMN_SIZE;
                    selectedPartyIndex = currentColumn * COLUMN_SIZE + rowInColumn;
                }

                // Left/Right to move between columns
                if (kDown & HidNpadButton_Left) {
                    if (currentColumn == 1) {
                        // Move from right column to left column, same row
                        selectedPartyIndex = rowInColumn;
                    }
                }
                if (kDown & HidNpadButton_Right) {
                    if (currentColumn == 0) {
                        // Move from left column to right column, same row
                        selectedPartyIndex = COLUMN_SIZE + rowInColumn;
                    }
                }

                // A button to view details
                if (kDown & HidNpadButton_A) {
                    // Only open if there's a pokemon in the selected slot
                    if (selectedPartyIndex >= 0 && selectedPartyIndex < static_cast<int>(trainer.party.size())) {
                        const Pokemon::Pokemon* pokemon = trainer.party[selectedPartyIndex].get();
                        if (pokemon && pokemon->speciesID() != 0) {  // Not empty
                            pokemonDetailsActive = true;
                            pokemonDetailsIsParty = true;
                            pokemonDetailsPartyIndex = selectedPartyIndex;
                            pokemonDetailsCategory = 0;
                            pokemonDetailsSelectedStat = 0;
                            pokemonDetailsEditing = false;
                        }
                    }
                }
            }

            return;  // Don't process other inputs while in detail view
        }

        // Normal mode navigation (not in detail view)
        if (kDown & HidNpadButton_B) {
            // Check for unsaved changes
            if (hasUnsavedChanges && !saveConfirmActive) {
                // Prompt to save changes before going back
                exitingWithUnsavedChanges = true;
                exitingViaPlus = false;  // Exiting via B button (go back)
                saveConfirmActive = true;
                return;
            }
            // No unsaved changes or already handled, go back immediately
            goBack = true;
        }

        // Up/Down to navigate between modes
        if (kDown & HidNpadButton_Up) {
            int modeIndex = static_cast<int>(selectedMode);
            modeIndex = (modeIndex - 1 + 3) % 3;  // 3 modes total
            selectedMode = static_cast<ViewMode>(modeIndex);
            currentPage = 0;  // Reset page when switching modes
        }
        if (kDown & HidNpadButton_Down) {
            int modeIndex = static_cast<int>(selectedMode);
            modeIndex = (modeIndex + 1) % 3;  // 3 modes total
            selectedMode = static_cast<ViewMode>(modeIndex);
            currentPage = 0;  // Reset page when switching modes
        }

        // A button to enter detail view (for Items/Boxes/Party modes)
        if (kDown & HidNpadButton_A) {
            if (selectedMode == ViewMode::Items || selectedMode == ViewMode::Boxes) {
                detailViewActive = true;
                selectedItemIndex = 0;
                currentPage = 0;
            } else if (selectedMode == ViewMode::Party) {
                // For Party mode, open detail view for party Pokemon selection
                detailViewActive = true;
                selectedPartyIndex = 0;
            }
        }

        // L/R to navigate categories (Items mode only, when not in detail view)
        if (selectedMode == ViewMode::Items) {
            // L/R to change categories (but not when in edit dialog)
            if (kDown & HidNpadButton_L) {
                switch(trainer.getGameGroup()) {
                    case GameVersion::ZA: {
                        selectedCategory = (selectedCategory - 1 + POUCH_COUNT9_LZA) % POUCH_COUNT9_LZA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SWSH: {
                        selectedCategory = (selectedCategory - 1 + 9) % 9; // TODO: Need to add global variable for this value
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    default: break;
                }
            }
            if (kDown & HidNpadButton_R) {
                switch(trainer.getGameGroup()) {
                    case GameVersion::ZA: {
                        selectedCategory = (selectedCategory + 1) % POUCH_COUNT9_LZA;
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    case GameVersion::SWSH: {
                        selectedCategory = (selectedCategory + 1) % 9; // TODO: Need to add global variable for this value
                        currentPage = 0;
                        selectedItemIndex = 0;
                        break;
                    }
                    default: break;
                }
            }
        }

        // L/R to navigate boxes (Boxes mode only, when not in detail view)
        if (selectedMode == ViewMode::Boxes) {
            if (kDown & HidNpadButton_L) {
                selectedBoxIndex = (selectedBoxIndex - 1 + 32) % 32;  // 32 boxes
            }
            if (kDown & HidNpadButton_R) {
                selectedBoxIndex = (selectedBoxIndex + 1) % 32;  // 32 boxes
            }
        }
    }

    void TrainerViewScreen::draw(PKSEFramebuffer& fb) {
        fb.clear(Colors::Background);

        // Draw title bar
        fb.drawFilledRect(0, 0, fb.getWidth(), 60, Colors::Panel);
        std::string titleText = "PKSE - " + titleName;
        fb.drawText(20, 20, titleText, Colors::Text);
        fb.drawRect(0, 0, fb.getWidth(), 60, Colors::Border);

        int contentPanelWidth = fb.getWidth() - CONTENT_PANEL_X;

        // Draw trainer info on the left (top)
        Panels::drawTrainerInfo(fb, trainer, LEFT_PANEL_X, LEFT_PANEL_Y, LEFT_TRAINER_INFO_PANEL_WIDTH, LEFT_TRAINER_INFO_PANEL_HEIGHT);

        // Calculate mode selector Y position (below trainer info with spacing)
        int modeSelectorY = LEFT_PANEL_Y + LEFT_TRAINER_INFO_PANEL_HEIGHT + 10;

        // Draw mode selector on the left (below trainer info)
        Panels::drawModeSelector(fb, static_cast<int>(selectedMode), 0, modeSelectorY, LEFT_VIEW_MODE_PANEL_WIDTH, LEFT_VIEW_MODE_PANEL_HEIGHT);

        // Draw content on the right based on selected mode
        switch (selectedMode) {
            case ViewMode::Party:
                Panels::drawPartyPokemon(fb, trainer.party, trainer.ID32, CONTENT_PANEL_X, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT, detailViewActive ? selectedPartyIndex : -1);
                break;
            case ViewMode::Boxes:
                Panels::drawBoxPokemon(*this, fb, CONTENT_PANEL_X, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                break;
            case ViewMode::Items:
                Panels::drawItems(*this, fb, CONTENT_PANEL_X, CONTENT_PANEL_Y, contentPanelWidth, CONTENT_PANEL_HEIGHT);
                break;
        }

        // Draw instructions
        std::string instructions;
        if (statEditDialogActive) {
            if (statEditMode == Dialogs::StatEditMode::EV) {
                instructions = "L/R: 选择项  |  方向键: +/- 数值  |  ZL/ZR: +/-100  |  A: 确认  |  B: 取消";
            } else {
                instructions = "L/R: 选择项  |  方向键: +/- 数值  |  A: 确认  |  B: 取消";
            }
        } else if (itemEditDialogActive) {
            instructions = "左/右: +/-1  |  上/下: +/-10  |  ZL/ZR: +/-100  |  A: 确认  |  B: 取消";
        } else if (saveConfirmActive) {
            instructions = "A: 保存更改  |  B: 取消";
        } else if (detailViewActive) {
            if (selectedMode == ViewMode::Items) {
                instructions = "方向键: 选择道具  |  A: 编辑数量  |  左/右: 列/页  |  L/R: 类别  |  B: 返回  |  X: 保存  |  +: 退出";
            } else if (selectedMode == ViewMode::Boxes) {
                if (pokemonDetailsActive) {
                    if (pokemonDetailsCategory == 0) { // Main
                        instructions = pokemonDetailsEditing
                            ? pokemonDetailsSelectedField == 3
                                ? "上/下: 选择项 | A: 编辑项 |  B: 返回  |  X: 保存  |  +: 退出"
                                : "上/下: 选择项 | B: 返回  |  X: 保存  |  +: 退出"
                            : "上/下: 选择类别 | A: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                    else if (pokemonDetailsCategory == 2) { // Stats
                        instructions = pokemonDetailsEditing
                            ? "上/下: 选择项  |  A: 编辑项 |  B: 返回  |  X: 保存  |  +: 退出"
                            : "上/下: 选择类别  |  A: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                    else {
                        instructions = "上/下: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                }
                else {
                    instructions = "方向键: 导航格子  |  L/R: 切换盒子  |  A: 查看详情  |  B: 返回  |  X: 保存  |  +: 退出";
                }
            } else if(selectedMode == ViewMode::Party) { // TODO: There HAS to be a better way of doing this without all of the if/else conditionals... probably will look into this at some point.
                if (pokemonDetailsActive) {
                    if (pokemonDetailsCategory == 0) { // Main
                        instructions = pokemonDetailsEditing
                            ? pokemonDetailsSelectedField == 3
                                ? "上/下: 选择项 | A: 编辑项 |  B: 返回  |  X: 保存  |  +: 退出"
                                : "上/下: 选择项 | B: 返回  |  X: 保存  |  +: 退出"
                            : "上/下: 选择类别 | A: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                    else if (pokemonDetailsCategory == 2) { // Stats
                        instructions = pokemonDetailsEditing
                            ? "上/下: 选择项  |  A: 编辑项 |  B: 返回  |  X: 保存  |  +: 退出"
                            : "上/下: 选择类别  |  A: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                    else {
                        instructions = "上/下: 选择类别  |  B: 关闭  |  X: 保存  |  +: 退出";
                    }
                }
                else {
                    instructions = "方向键: 导航格子  |  A: 查看详情  |  B: 返回  |  X: 保存  |  +: 退出";
                }
            }
        } else {
            if (selectedMode == ViewMode::Items) {
                instructions = "上/下: 选择模式  |  L/R: 类别  |  A: 进入详情  |  B: 返回  |  X: 保存  |  +: 退出";
            } else if (selectedMode == ViewMode::Boxes) {
                instructions = "上/下: 选择模式  |  L/R: 切换盒子  |  A: 进入详情  |  B: 返回  |  X: 保存  |  +: 退出";
            } else {
                instructions = "上/下: 选择模式  |  A: 进入详情  |  B: 返回  |  X: 保存  |  +: 退出";
            }
        }
        fb.drawText(50, 680, instructions, Colors::TextDim);

        // Draw dialogs on top of everything (Modals first, then dialogs)
        if (pokemonDetailsActive) {
            Modals::drawPokemonDetailsModal(*this, fb);
        }
        if (itemEditDialogActive) {
            Dialogs::drawItemEditDialog(*this, fb);
        }
        if (statEditDialogActive) {
            Dialogs::drawStatEditDialog(*this, fb);
        }
        if (saveConfirmActive) {
            Dialogs::drawSaveConfirmDialog(*this, fb);
        }
    }
}
