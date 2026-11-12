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
    "11.01 east_of_snorlax_wall from CeladonCity (48, 12) facing south to Route16 (33, 14) facing west",
    "11.01   walk west 25 -> CeladonCity (23, 12)",
    "11.01   walk south 4 (to wall) -> CeladonCity (23, 16)",
    "11.01   walk west 19 (to wall) -> CeladonCity (4, 16)",
    "11.01   walk south 8 (to wall) -> CeladonCity (4, 24)",
    "11.01   walk west 19 (to wall) -> Route16 (33, 14) [connection 5]",
    "11.01 east_of_snorlax from Route16 (33, 14) facing west to Route16 (32, 13) facing west",
    "11.01   walk north 1 -> Route16 (33, 13)",
    "11.01   walk west 1 -> Route16 (32, 13)",
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
    "11.03 below_safari_counter from FuchsiaCity (24, 6) facing north to FuchsiaCity_SafariZone_Entrance (4, 4) facing north",
    "11.03   walk north 1 (warp) -> FuchsiaCity_SafariZone_Entrance (4, 7)",
    "11.03   walk north 3 -> FuchsiaCity_SafariZone_Entrance (4, 4) [walk only]",
    "11.03 center_to_east from SafariZone_Center (26, 30) facing north to SafariZone_East (9, 27) facing east",
    "11.03   walk north 1 -> SafariZone_Center (26, 29)",
    "11.03   walk east 16 (to wall) -> SafariZone_Center (42, 29)",
    "11.03   walk north 14 (to wall) -> SafariZone_Center (42, 15)",
    "11.03   walk east 2 (warp) -> SafariZone_East (8, 26)",
    "11.03   walk south 1 (to wall) -> SafariZone_East (8, 27)",
    "11.03   walk east 1 -> SafariZone_East (9, 27)",
    "11.03 below_full_restore from SafariZone_East (37, 16) facing east to SafariZone_East (37, 16) facing north",
    "11.03   face north -> SafariZone_East (37, 16)",
    "11.03 into_north_area from SafariZone_East (9, 9) facing west to SafariZone_North (48, 31) facing west",
    "11.03   walk west 2 (warp) -> SafariZone_North (48, 31)",
    "11.03 into_west_area from SafariZone_North (11, 33) facing south to SafariZone_West (31, 5) facing south",
    "11.03   walk south 2 (warp) -> SafariZone_West (31, 5)",
    "11.03 below_gold_teeth from SafariZone_West (28, 15) facing west to SafariZone_West (28, 15) facing north",
    "11.03   face north -> SafariZone_West (28, 15)",
    "11.03 below_secret_house_attendant from SafariZone_West (28, 15) facing north to SafariZone_SecretHouse (6, 6) facing north",
    "11.03   walk west 1 (to wall) -> SafariZone_West (27, 15)",
    "11.03   walk north 5 -> SafariZone_West (27, 10)",
    "11.03   walk west 15 -> SafariZone_West (12, 10)",
    "11.03   walk north 3 (warp) -> SafariZone_SecretHouse (4, 9)",
    "11.03   walk north 3 (to wall) -> SafariZone_SecretHouse (4, 6) [walk only]",
    "11.03   walk east 2 -> SafariZone_SecretHouse (6, 6) [walk only]",
    "11.03   face north -> SafariZone_SecretHouse (6, 6) [walk only]",
    "11.03 out_of_secret_house from SafariZone_SecretHouse (6, 6) facing north to SafariZone_West (12, 8) facing south",
    "11.03   walk west 2 -> SafariZone_SecretHouse (4, 6) [walk only]",
    "11.03   walk south 4 (warp) -> SafariZone_West (12, 8) [walk only]",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
