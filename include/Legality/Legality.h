/**
 * Legality.h - PKSE legality checker (Layers 1+2, informational).
 *
 * analyze() runs a set of structural (Layer 1) and internal-consistency (Layer 2)
 * checks against a decrypted Pokemon and returns human-readable issues. It is
 * INFORMATIONAL only (no auto-fix) and deliberately conservative: checks that need
 * data PKSE does not have (per-species legal abilities, learnsets, form counts,
 * gender ratios, encounter tables) are NOT performed. A clean report therefore means
 * "no problems found", never a guarantee of full legality.
 *
 * No RTTI / no exceptions: every check calls base Pokemon virtuals; per-gen
 * differences branch on the game group / capability flags, never on concrete type.
 */

#ifndef LEGALITY_LEGALITY_H
#define LEGALITY_LEGALITY_H

#include <cstdint>
#include <string>
#include <vector>

#include "Enums/GameVersion.h"

namespace Pokemon { class Pokemon; }  // fwd decl — no heavy include

namespace Legality {

    enum class Severity : uint8_t { Info, Warning, Invalid };

    struct Issue {
        Severity severity;
        std::string text;
    };

    struct Report {
        std::vector<Issue> issues;

        /// Number of Warning+Invalid issues (Info notes are not counted as problems).
        int problemCount() const noexcept {
            int n = 0;
            for (const auto& i : issues) if (i.severity != Severity::Info) ++n;
            return n;
        }
        bool ok() const noexcept { return problemCount() == 0; }
    };

    /// Analyze a decrypted Pokemon and return its legality issues (informational).
    /// originGroup = the save's format group (Trainer::getGameGroup()). Returns an
    /// empty report for an empty slot (species 0).
    Report analyze(const Pokemon::Pokemon& pk, Enums::GameVersion originGroup);
}

#endif
