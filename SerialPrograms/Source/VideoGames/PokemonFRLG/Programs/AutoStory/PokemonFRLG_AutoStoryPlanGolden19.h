/*  FRLG AutoStory Plan Golden, Segment 19
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 19.01a and after (Route 22's rival, Route 23,
 *  Victory Road and Indigo Plateau), in the story's order, after
 *  PokemonFRLG_AutoStoryPlanGolden17.h's 17.02 to 17.04.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden19_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden19_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines of 19.01a and after.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_19[] = {
    "19.01a viridian_mart_clerk from ViridianCity_PokemonCenter_1F (7, 4) facing north to ViridianCity_Mart (4, 3) facing west",
    "19.01a   walk south 5 (warp) -> ViridianCity (26, 27) [walk only]",
    "19.01a   walk south 2 -> ViridianCity (26, 29)",
    "19.01a   walk east 10 -> ViridianCity (36, 29)",
    "19.01a   walk north 10 (warp) -> ViridianCity_Mart (4, 7)",
    "19.01a   walk north 4 -> ViridianCity_Mart (4, 3) [walk only]",
    "19.01a   face west -> ViridianCity_Mart (4, 3) [walk only]",
    "19.01 route22_east_of_trigger from ViridianCity_Mart (4, 3) facing west to Route22 (34, 5) facing west",
    "19.01   walk south 5 (warp) -> ViridianCity (36, 20) [walk only]",
    "19.01   walk west 13 -> ViridianCity (23, 20)",
    "19.01   walk north 1 (to wall) -> ViridianCity (23, 19)",
    "19.01   walk west 29 (to wall) -> Route22 (42, 9) [connection 24]",
    "19.01   walk south 5 -> Route22 (42, 14)",
    "19.01   walk west 6 -> Route22 (36, 14)",
    "19.01   walk north 9 -> Route22 (36, 5) [wild]",
    "19.01   walk west 2 -> Route22 (34, 5)",
    "19.02 above_route22_ledge from Route22 (33, 5) facing west to Route22 (18, 7) facing south",
    "19.02   walk west 15 -> Route22 (18, 5)",
    "19.02   walk south 2 -> Route22 (18, 7)",
    "19.02 below_gate_guard_trigger from Route22 (18, 9) facing south to Route22_NorthEntrance (7, 3) facing north",
    "19.02   walk south 5 -> Route22 (18, 14) [wild]",
    "19.02   walk west 14 (to wall) -> Route22 (4, 14)",
    "19.02   walk north 4 -> Route22 (4, 10)",
    "19.02   walk east 6 -> Route22 (10, 10)",
    "19.02   walk north 4 (to wall) -> Route22 (10, 6)",
    "19.02   walk west 2 -> Route22 (8, 6)",
    "19.02   walk north 1 (warp) -> Route22_NorthEntrance (7, 10)",
    "19.02   walk north 7 -> Route22_NorthEntrance (7, 3) [walk only]",
    "19.03 cascade_guard_approach from Route23 (8, 153) facing north to Route23 (13, 150) facing north",
    "19.03   walk north 3 -> Route23 (8, 150)",
    "19.03   walk east 5 -> Route23 (13, 150)",
    "19.03   face north -> Route23 (13, 150)",
    "19.03 thunder_guard_approach from Route23 (13, 149) facing north to Route23 (7, 141) facing north",
    "19.03   walk north 8 -> Route23 (13, 141)",
    "19.03   walk west 6 -> Route23 (7, 141)",
    "19.03   face north -> Route23 (7, 141)",
    "19.03 rainbow_guard_approach from Route23 (7, 140) facing north to Route23 (12, 124) facing north",
    "19.03   walk north 9 (to wall) -> Route23 (7, 131)",
    "19.03   walk west 5 (to wall) -> Route23 (2, 131)",
    "19.03   walk north 7 -> Route23 (2, 124)",
    "19.03   walk east 10 -> Route23 (12, 124)",
    "19.03   face north -> Route23 (12, 124)",
    "19.03 soul_shore from Route23 (12, 123) facing north to Route23 (13, 118) facing north",
    "19.03   walk north 5 (to wall) -> Route23 (12, 118)",
    "19.03   walk east 1 -> Route23 (13, 118)",
    "19.03   face north -> Route23 (13, 118)",
    "19.03 soul_guard_approach from Route23 (13, 117) facing north to Route23 (13, 113) facing north",
    "19.03   walk north 4 -> Route23 (13, 113) [surfing] [wild]",
    "19.03 marsh_guard_approach from Route23 (13, 112) facing north to Route23 (9, 95) facing north",
    "19.03   walk north 13 (to wall) (lands after 9) -> Route23 (13, 99) [surfing] [wild]",
    "19.03   walk west 4 -> Route23 (9, 99)",
    "19.03   surf north -> Route23 (9, 98) [wild]",
    "19.03   walk north 3 -> Route23 (9, 95) [surfing] [wild]",
    "19.03 land_below_volcano from Route23 (9, 94) facing north to Route23 (17, 77) facing north",
    "19.03   walk north 17 (to wall) (lands after 16) -> Route23 (9, 77) [surfing] [wild]",
    "19.03   walk east 8 -> Route23 (17, 77)",
    "19.03   face north -> Route23 (17, 77)",
    "19.03 volcano_guard_approach from Route23 (17, 77) facing north to Route23 (12, 62) facing north",
    "19.03   walk north 16 (to wall) -> Route23 (17, 61) [wild]",
    "19.03   walk south 1 -> Route23 (17, 62)",
    "19.03   walk west 5 -> Route23 (12, 62)",
    "19.03   face north -> Route23 (12, 62)",
    "19.03 earth_guard_approach from Route23 (12, 61) facing north to Route23 (5, 36) facing north",
    "19.03   walk north 14 (to wall) -> Route23 (12, 47)",
    "19.03   walk west 3 (to wall) -> Route23 (9, 47)",
    "19.03   walk north 5 (to wall) -> Route23 (9, 42)",
    "19.03   walk west 4 -> Route23 (5, 42)",
    "19.03   walk north 6 -> Route23 (5, 36)",
    "19.03 victory_road_mouth from Route23 (5, 35) facing north to Route23 (5, 29) facing north",
    "19.03   walk north 6 -> Route23 (5, 29)",
    "19.05 indigo_door from Route23 (18, 28) facing south to IndigoPlateau_Exterior (11, 7) facing north",
    "19.05   walk south 4 (to wall) -> Route23 (18, 32)",
    "19.05   walk east 3 (to wall) -> Route23 (21, 32)",
    "19.05   walk north 14 -> Route23 (21, 18)",
    "19.05   walk west 7 -> Route23 (14, 18)",
    "19.05   walk north 13 (to wall) -> Route23 (14, 5)",
    "19.05   walk east 1 (to wall) -> Route23 (15, 5)",
    "19.05   walk north 4 (to wall) -> Route23 (15, 1)",
    "19.05   walk west 1 (to wall) -> Route23 (14, 1)",
    "19.05   walk south 1 (to wall) -> Route23 (14, 2)",
    "19.05   walk west 3 -> Route23 (11, 2)",
    "19.05   walk north 15 -> IndigoPlateau_Exterior (11, 7) [connection 3]",
    "19.05 indigo_nurse from IndigoPlateau_Exterior (11, 7) facing north to IndigoPlateau_PokemonCenter_1F (13, 12) facing north",
    "19.05   walk north 1 (warp) -> IndigoPlateau_PokemonCenter_1F (11, 16)",
    "19.05   walk north 4 (to wall) -> IndigoPlateau_PokemonCenter_1F (11, 12) [walk only]",
    "19.05   walk east 2 -> IndigoPlateau_PokemonCenter_1F (13, 12) [walk only]",
    "19.05   face north -> IndigoPlateau_PokemonCenter_1F (13, 12) [walk only]",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
