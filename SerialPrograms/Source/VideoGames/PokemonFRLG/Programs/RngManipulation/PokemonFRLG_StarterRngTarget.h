/*  Starter RNG Target
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The starter Starter RNG aims for: its species and their stats, the target
 *  advances and the program's stats.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngTarget_H
#define PokemonAutomation_PokemonFRLG_StarterRngTarget_H

#include <cstdint>
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "Pokemon/Pokemon_StatsCalculation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Selectable Kanto starter species.
enum class StarterSpecies{
    bulbasaur,
    squirtle,
    charmander
};

//  The species' base stats.
BaseStats starter_base_stats(StarterSpecies starter);
//  The EVs the rival's starter gives when the player's "starter" beats it.
EVs rival_ev_yield(StarterSpecies starter);
//  After taking "starter" from its ball and dodging the rival, the steps
//  left that line the player up with the lab's door.
int steps_left_to_lab_door(StarterSpecies starter);


//  Statistical counters for Starter RNG attempts.
struct StarterRngStats : public StatsTracker{
    StarterRngStats();

    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonshiny;
    std::atomic<uint64_t>& wildshinies;
    std::atomic<uint64_t>& errors;
};


//  Target criteria for starter manipulation.
struct StarterRngTarget{
    StarterSpecies starter = StarterSpecies::bulbasaur;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The seed's entry in the seeds database (found).
    SeedMatch seed_match;
};

//  Where an attempt is after a reset: the resets so far, and the
//  calibration that reset was timed with.
struct StarterRngProgress{
    uint64_t resets = 0;
    RngCalibrations calibrations{};
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
