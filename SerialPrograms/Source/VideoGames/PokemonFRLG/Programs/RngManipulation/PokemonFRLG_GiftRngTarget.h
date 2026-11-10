/*  Gift RNG Target
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The gift Gift RNG aims for: its species, the target advances, and the
 *  program's stats.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_GiftRngTarget_H
#define PokemonAutomation_PokemonFRLG_GiftRngTarget_H

#include <cstdint>
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "Pokemon/Pokemon_StatsCalculation.h"
#include "PokemonFRLG_BlindNavigation.h"
#include "PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the search needs to know about a gift: its base stats, its gender
//  threshold (-1 if genderless) and the level it is received at (0 to keep
//  the level read from its summary).
struct GiftRngSpecies{
    BaseStats base_stats{};
    int16_t gender_threshold = -1;
    uint8_t level = 0;
};

//  The gift that "target" gives. Targets that aren't gifts get the defaults.
GiftRngSpecies gift_rng_species(PokemonFRLG_RngTarget target);


//  Statistics tracker for Gift RNG attempts.
struct GiftRngStats : public StatsTracker{
    GiftRngStats();

    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonshiny;
    std::atomic<uint64_t>& errors;
};


//  Target seed and advances for Gift RNG manipulation.
struct GiftRngTarget{
    PokemonFRLG_RngTarget target = PokemonFRLG_RngTarget::magikarp;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The seed's entry in the seeds database (found).
    SeedMatch seed_match;
};

//  Where an attempt is after a reset: the resets so far, and the
//  calibration that reset was timed with.
struct GiftRngProgress{
    uint64_t resets = 0;
    RngCalibrations calibrations{};
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
