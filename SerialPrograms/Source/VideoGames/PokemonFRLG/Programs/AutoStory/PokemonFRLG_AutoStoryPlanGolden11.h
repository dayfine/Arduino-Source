/*  FRLG AutoStory Plan Golden, Segment 11
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines from segment 11 on, after
 *  PokemonFRLG_AutoStoryPlanGolden10.h's. The test reads FRLG_PLAN_GOLDEN,
 *  FRLG_PLAN_GOLDEN_10, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden11_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden11_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines from segment 11 on, after FRLG_PLAN_GOLDEN_10's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_11[] = {
    "11.01 out_of_fuji_house from LavenderTown_VolunteerPokemonHouse (3, 4) facing north to LavenderTown (10, 12) facing south",
    "11.01   walk east 1 (to wall) -> LavenderTown_VolunteerPokemonHouse (4, 4) [walk only]",
    "11.01   walk south 4 (warp) -> LavenderTown (10, 12) [walk only]",
    "11.01 east_of_snorlax from CeladonCity (48, 12) facing south to Route16 (32, 13) facing west",
    "11.01   walk west 25 -> CeladonCity (23, 12)",
    "11.01   walk south 4 (to wall) -> CeladonCity (23, 16)",
    "11.01   walk west 19 (to wall) -> CeladonCity (4, 16)",
    "11.01   walk south 7 -> CeladonCity (4, 23)",
    "11.01   walk west 20 -> Route16 (32, 13) [connection 5]",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
