/**
 * An import is a bulk action -- a real PKSM bank routinely carries a few hundred Pokemon -- so it
 * gets a preview rather than firing on the file pick. The preview is not decoration: the file has
 * already been fully parsed by then, so it can state exactly how many Pokemon land, how many boxes
 * they take, and how many records could not be read at all. Cancelling at that point has cost
 * nothing; nothing has been written.
 */
#ifndef UI_DIALOGS_PKSM_IMPORT_DIALOG_H
#define UI_DIALOGS_PKSM_IMPORT_DIALOG_H

#include <string>

#include "Trainer/PKSMBank.h"

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Dialogs
    {

        struct PKSMImportState
        {
            Trainer::PKSMBankImport importer;
            std::string fileName;       // leaf name of the chosen .bnk, for the headings
            bool previewActive = false; // "here is what this file holds" -- A imports, B cancels
            bool resultActive = false;  // the outcome, or a reason the file could not be used
            bool committed = false;     // the result screen is reporting a real import

            void reset()
            {
                importer.clear();
                fileName.clear();
                previewActive = resultActive = committed = false;
            }
        };

        /// Pre-import summary: per-generation counts, what will be skipped, where it will land.
        void drawPKSMImportPreview(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);

        /// Post-import summary -- also the "this file can't be used" and "nothing importable" screen.
        void drawPKSMImportResult(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif
