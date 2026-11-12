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
    "11.01 east_of_cycling_road_gate from Route16 (32, 13) facing west to Route16 (28, 13) facing west",
    "11.01   walk west 4 -> Route16 (28, 13)",
    "11.02 east_of_bikers from Route16 (28, 13) facing west to Route16 (17, 13) facing west",
    "11.02   walk west 2 (warp) -> Route16_NorthEntrance_1F (11, 12)",
    "11.02   walk west 11 (warp) -> Route16 (20, 13)",
    "11.02   walk west 3 -> Route16 (17, 13)",
    "11.02 safari_zone_door from Route18 (8, 10) facing south to FuchsiaCity (24, 6) facing north",
    "11.02   walk north 1 -> Route18 (8, 9)",
    "11.02   walk east 34 (warp) -> Route18_EastEntrance_1F (1, 6)",
    "11.02   walk east 11 (warp) -> Route18 (48, 9)",
    "11.02   walk east 18 (to wall) -> FuchsiaCity (6, 19) [connection 12]",
    "11.02   walk south 6 (to wall) -> FuchsiaCity (6, 25)",
    "11.02   walk east 23 -> FuchsiaCity (29, 25)",
    "11.02   walk north 8 (to wall) -> FuchsiaCity (29, 17)",
    "11.02   walk east 7 -> FuchsiaCity (36, 17)",
    "11.02   walk south 1 (to wall) -> FuchsiaCity (36, 18)",
    "11.02   walk east 9 (to wall) -> FuchsiaCity (45, 18)",
    "11.02   walk north 13 (to wall) -> FuchsiaCity (45, 5)",
    "11.02   walk west 17 (to wall) -> FuchsiaCity (28, 5)",
    "11.02   walk south 1 (to wall) -> FuchsiaCity (28, 6)",
    "11.02   walk west 4 -> FuchsiaCity (24, 6)",
    "11.02   face north -> FuchsiaCity (24, 6)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
