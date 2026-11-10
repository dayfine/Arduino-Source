/*  Starter RNG Timings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Starter RNG's waits as the RNG plan computes them: its smallest Advances
 *  setting, its title screen and in-game waits before calibration, and the
 *  margins the plan keeps from Starter RNG's limits.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngTimings_H
#define PokemonAutomation_PokemonFRLG_StarterRngTimings_H

#include <cstdint>
#include "PokemonFRLG_RngTargetSearch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Starter RNG's smallest Advances setting.
inline constexpr uint64_t STARTER_RNG_MIN_ADVANCES = 940;

//  Starter RNG's title screen wait, before calibration: the seed list's
//  delay plus this (FIXED_SEED_OFFSET in StarterRng's attempts).
inline constexpr int64_t STARTER_RNG_FIXED_SEED_OFFSET_MS = -845;
//  The shortest title screen wait the plan uses. Starter RNG refuses waits
//  under 29,650 ms, and its docs (pokemonautomation.github.io,
//  docs/Programs/PokemonFRLG/StarterRng.md, "Avoid low seed delays when
//  possible") warn that waits near 30,500 ms can miss the title screen
//  press. 31,500 ms keeps a second clear of that for the seed calibration.
inline constexpr int64_t STARTER_RNG_MIN_SEED_WAIT_MS = 31500;
//  Starter RNG's title screen wait for `seed`, as prepare_timings() makes it
//  before calibration.
int64_t starter_rng_seed_wait_ms(const InitialSeedEntry& seed);
//  Whether the wait is at least STARTER_RNG_MIN_SEED_WAIT_MS. (Console B,
//  2026-11-02, FireRed English: seeds 11A3 and 405C, at 30,473 and
//  30,488 ms in the table, are 29,628 and 29,643 ms waits, which Starter
//  RNG refuses.)
bool starter_rng_can_wait_for(const InitialSeedEntry& seed);

//  Starter RNG's Continue Screen frames, taken off the advances before the
//  in-game wait (CONTINUE_SCREEN_FRAMES in StarterRng's attempts).
inline constexpr uint64_t STARTER_RNG_CONTINUE_SCREEN_FRAMES = 200;
//  Starter RNG's in-game wait for `advances`, before calibration. It
//  refuses one under 7,500 ms, which STARTER_RNG_MIN_ADVANCES just makes.
int64_t starter_rng_ingame_ms(uint64_t advances);
//  Room for the in-game calibration, in advances (2 per frame of wait): the
//  calibration moves the wait, and it went to -720 after one miss on console
//  B (2026-11-02, seed B579 at 967 advances: an 8 s wait cut to 1.7 s). The
//  plan's targets are at least this many advances past Starter RNG's
//  minimum. To tune from the logs' calibrations.
inline constexpr uint64_t STARTER_RNG_INGAME_MARGIN_ADVANCES = 720;



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
