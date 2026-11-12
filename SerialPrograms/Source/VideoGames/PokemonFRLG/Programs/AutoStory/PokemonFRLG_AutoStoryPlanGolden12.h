/*  FRLG AutoStory Plan Golden, Segment 12
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines from segment 12 on, after
 *  PokemonFRLG_AutoStoryPlanGolden11.h's. The test reads FRLG_PLAN_GOLDEN,
 *  FRLG_PLAN_GOLDEN_10, FRLG_PLAN_GOLDEN_11, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden12_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden12_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines from segment 12 on, after FRLG_PLAN_GOLDEN_11's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_12[] = {
    "12.01 face_center_door from FuchsiaCity (25, 32) facing south to FuchsiaCity (25, 32) facing north",
    "12.01   face north -> FuchsiaCity (25, 32)",
    "12.01 west_of_east_ledge from FuchsiaCity_PokemonCenter_1F (0, 2) facing north to FuchsiaCity (27, 32) facing east",
    "12.01   walk south 3 (to wall) -> FuchsiaCity_PokemonCenter_1F (0, 5) [walk only]",
    "12.01   walk east 7 -> FuchsiaCity_PokemonCenter_1F (7, 5) [walk only]",
    "12.01   walk south 4 (warp) -> FuchsiaCity (25, 32) [walk only]",
    "12.01   walk east 2 -> FuchsiaCity (27, 32)",
    "12.01 below_mart_tree from FuchsiaCity (29, 32) facing east to FuchsiaCity (24, 23) facing north",
    "12.01   walk north 9 -> FuchsiaCity (29, 23)",
    "12.01   walk west 5 -> FuchsiaCity (24, 23)",
    "12.01   face north -> FuchsiaCity (24, 23)",
    "12.01 fuchsia_mart_wall from FuchsiaCity (24, 22) facing north to FuchsiaCity_Mart (4, 2) facing north",
    "12.01   walk north 6 -> FuchsiaCity (24, 16)",
    "12.01   walk west 13 -> FuchsiaCity (11, 16)",
    "12.01   walk north 1 (warp) -> FuchsiaCity_Mart (4, 7)",
    "12.01   walk north 5 (to wall) -> FuchsiaCity_Mart (4, 2) [walk only]",
    "12.01 fuchsia_mart_clerk from FuchsiaCity_Mart (4, 2) facing north to FuchsiaCity_Mart (4, 3) facing west",
    "12.01   walk south 1 -> FuchsiaCity_Mart (4, 3) [walk only]",
    "12.01   face west -> FuchsiaCity_Mart (4, 3) [walk only]",
    "12.01 above_mart_tree from FuchsiaCity_Mart (4, 3) facing west to FuchsiaCity (24, 21) facing south",
    "12.01   walk south 5 (warp) -> FuchsiaCity (11, 16) [walk only]",
    "12.01   walk south 1 (to wall) -> FuchsiaCity (11, 17)",
    "12.01   walk east 13 (to wall) -> FuchsiaCity (24, 17)",
    "12.01   walk south 4 -> FuchsiaCity (24, 21)",
    "12.01 gym_east_wall from FuchsiaCity (24, 22) facing south to FuchsiaCity_Gym (14, 21) facing north",
    "12.01   walk south 3 (to wall) -> FuchsiaCity (24, 25)",
    "12.01   walk west 22 (to wall) -> FuchsiaCity (2, 25)",
    "12.01   walk south 12 (to wall) -> FuchsiaCity (2, 37)",
    "12.01   walk east 7 -> FuchsiaCity (9, 37)",
    "12.01   walk north 5 (warp) -> FuchsiaCity_Gym (7, 21)",
    "12.01   walk east 7 (to wall) -> FuchsiaCity_Gym (14, 21) [walk only]",
    "12.01   face north -> FuchsiaCity_Gym (14, 21) [walk only]",
    "12.01 west_of_koga from FuchsiaCity_Gym (6, 13) facing south to FuchsiaCity_Gym (6, 13) facing east",
    "12.01   face east -> FuchsiaCity_Gym (6, 13) [walk only]",
    "12.02 below_warden from FuchsiaCity_Gym (6, 13) facing east to FuchsiaCity_WardensHouse (1, 6) facing north",
    "12.02   walk north 1 (to wall) -> FuchsiaCity_Gym (6, 12) [walk only]",
    "12.02   walk west 6 (to wall) -> FuchsiaCity_Gym (0, 12) [walk only]",
    "12.02   walk north 1 (to wall) -> FuchsiaCity_Gym (0, 11) [walk only]",
    "12.02   walk east 2 (to wall) -> FuchsiaCity_Gym (2, 11) [walk only]",
    "12.02   walk north 1 -> FuchsiaCity_Gym (2, 10) [walk only]",
    "12.02   walk east 7 (to wall) -> FuchsiaCity_Gym (9, 10) [walk only]",
    "12.02   walk north 8 (to wall) -> FuchsiaCity_Gym (9, 2) [walk only]",
    "12.02   walk east 5 (to wall) -> FuchsiaCity_Gym (14, 2) [walk only]",
    "12.02   walk south 19 (to wall) -> FuchsiaCity_Gym (14, 21) [walk only]",
    "12.02   walk west 7 -> FuchsiaCity_Gym (7, 21) [walk only] [tapped]",
    "12.02   walk south 1 (warp) -> FuchsiaCity (9, 33) [walk only]",
    "12.02   walk south 4 (to wall) -> FuchsiaCity (9, 37)",
    "12.02   walk west 7 (to wall) -> FuchsiaCity (2, 37)",
    "12.02   walk north 12 -> FuchsiaCity (2, 25)",
    "12.02   walk east 28 (to wall) -> FuchsiaCity (30, 25)",
    "12.02   walk south 7 -> FuchsiaCity (30, 32)",
    "12.02   walk east 3 -> FuchsiaCity (33, 32)",
    "12.02   walk north 1 (warp) -> FuchsiaCity_WardensHouse (6, 9)",
    "12.02   walk west 5 (to wall) -> FuchsiaCity_WardensHouse (1, 9) [walk only]",
    "12.02   walk north 3 (to wall) -> FuchsiaCity_WardensHouse (1, 6) [walk only]",
    "12.02 face_warden from FuchsiaCity_WardensHouse (3, 6) facing east to FuchsiaCity_WardensHouse (3, 6) facing north",
    "12.02   face north -> FuchsiaCity_WardensHouse (3, 6) [walk only]",
    "12.02 below_center_door from FuchsiaCity_WardensHouse (3, 6) facing north to FuchsiaCity (25, 32) facing north",
    "12.02   walk east 3 -> FuchsiaCity_WardensHouse (6, 6) [walk only]",
    "12.02   walk south 4 (warp) -> FuchsiaCity (33, 32) [walk only]",
    "12.02   walk west 4 -> FuchsiaCity (29, 32)",
    "12.02   walk north 7 -> FuchsiaCity (29, 25)",
    "12.02   walk west 27 (to wall) -> FuchsiaCity (2, 25)",
    "12.02   walk south 12 (to wall) -> FuchsiaCity (2, 37)",
    "12.02   walk east 9 (to wall) -> FuchsiaCity (11, 37)",
    "12.02   walk north 5 (to wall) -> FuchsiaCity (11, 32)",
    "12.02   walk east 14 -> FuchsiaCity (25, 32)",
    "12.02   face north -> FuchsiaCity (25, 32)",
    "12.03 outside_fuchsia_center from FuchsiaCity_PokemonCenter_1F (0, 2) facing north to FuchsiaCity (25, 32) facing south",
    "12.03   walk south 3 (to wall) -> FuchsiaCity_PokemonCenter_1F (0, 5) [walk only]",
    "12.03   walk east 7 -> FuchsiaCity_PokemonCenter_1F (7, 5) [walk only]",
    "12.03   walk south 4 (warp) -> FuchsiaCity (25, 32) [walk only]",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
