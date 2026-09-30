/**
 * TradeDonor.h - Using another save's trainer as the other side of a trade.
 *
 * A trade evolution from Generation 6 on leaves a handling trainer behind, and that residue is the
 * only evidence the trade happened -- see Pokemon/Trade.h. PKSE will not invent a person, so the
 * trainer comes from a save the user actually owns: this is the rule for whether a given save may
 * stand in, and the extraction of what the handler block needs from it.
 *
 * WHY IT LIVES IN THE TRAINER LAYER. It reads a Trainer and writes a Pokemon::TradePartner, and a
 * Pokemon-layer source must not include the trainer layer to reach it -- the same boundary
 * Conversion/Convert.cpp respects by forward-declaring the species table rather than including
 * Trainer.h.
 *
 * WHAT IT DELIBERATELY DOES NOT SUPPLY. **Geolocation stays empty.** Generations 6 and 7 record the
 * console country and region of each handler, and an all-zero history is perfectly legal -- but a
 * country/region pair that does not exist is NOT (PKHeX IGeoTrack.GetValidity ->
 * CountryDoesNotHaveRegion), and PKSE has no table of valid pairs to check one against. Writing
 * nothing is the same choice the Gen 3 OT name makes: leave the field blank rather than fill it
 * with something that cannot be verified.
 */
#ifndef TRAINER_TRADEDONOR_H
#define TRAINER_TRADEDONOR_H

#include <cstdint>

#include "Enums/GameVersion.h"
#include "Pokemon/Trade.h"

namespace Pokemon
{
    class Pokemon;
}

namespace Trainer
{

    class Trainer;

    /** Why a save cannot stand in as the other side of a trade. */
    enum class TradeDonorRefusal : uint8_t
    {
        None = 0,
        NoHandler,      ///< the Pokemon's format records no handler, so a trade leaves no trace
        CannotTrade,    ///< the two games cannot trade with each other
        SameTrainer,    ///< the save's trainer IS this Pokemon's OT -- a trade to yourself
        NameUnstorable, ///< the trainer's name will not fit the destination's handler field
    };

    /** A donor save's trainer, and whether it may be used. */
    struct TradeDonor
    {
        ::Pokemon::TradePartner partner;
        TradeDonorRefusal refusal = TradeDonorRefusal::None;

        bool isAccepted() const noexcept { return refusal == TradeDonorRefusal::None; }
    };

    /**
     * True when a Pokemon in `recordGroup`'s format could have been traded to `donorGroup`.
     *
     * Same group always, plus the two pairs PKSE splits that the games do not -- X/Y with
     * Omega Ruby/Alpha Sapphire, and Sun/Moon with Ultra Sun/Ultra Moon, which link freely.
     * **LEGENDS: ARCEUS TRADES WITH NOTHING**: it has no trading feature at all, which is also why
     * it needs no donor -- its four trade evolutions happen through the Linking Cord instead.
     */
    bool gamesCanTrade(Enums::GameVersion donorGroup, Enums::GameVersion recordGroup) noexcept;

    /**
     * True when a save of `titleVersion` could be the other side of a trade for `pokemon` -- the
     * test the donor title picker filters the console's saves with.
     *
     * Takes a concrete TITLE (Sword, Violet, Shining Pearl) rather than a group, because that is
     * what the console reports and what the picker draws. **A format with no handler answers false
     * for every title**: before Generation 6 a trade writes nothing to the record, so there is
     * nothing a donor could contribute and a picker would be asking a question with no answer.
     */
    bool titleCanTradeWith(Enums::GameVersion titleVersion, const ::Pokemon::Pokemon &pokemon);

    /** Reads `donor`'s trainer as a trade partner for `pokemon`, or says why it cannot be one. */
    TradeDonor buildTradeDonor(const Trainer &donor, const ::Pokemon::Pokemon &pokemon);

    /** One line naming the refusal, for the dialog that has to explain it. */
    const char *tradeDonorRefusalText(TradeDonorRefusal refusal) noexcept;
}

#endif  // TRAINER_TRADEDONOR_H
