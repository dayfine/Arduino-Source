/*  FRLG AutoStory Plan Golden, Segment 14
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines from segment 14 on, after
 *  PokemonFRLG_AutoStoryPlanGolden12.h's. The test reads FRLG_PLAN_GOLDEN,
 *  FRLG_PLAN_GOLDEN_10, FRLG_PLAN_GOLDEN_11, FRLG_PLAN_GOLDEN_12, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden14_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden14_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines from segment 14 on, after FRLG_PLAN_GOLDEN_12's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_14[] = {
    "14.01 below_center_door from CinnabarIsland (8, 4) facing south to CinnabarIsland (14, 12) facing north",
    "14.01   walk east 10 (to wall) -> CinnabarIsland (18, 4)",
    "14.01   walk south 4 (to wall) -> CinnabarIsland (18, 8)",
    "14.01   walk east 4 (to wall) -> CinnabarIsland (22, 8)",
    "14.01   walk south 4 -> CinnabarIsland (22, 12)",
    "14.01   walk west 8 -> CinnabarIsland (14, 12)",
    "14.01   face north -> CinnabarIsland (14, 12)",
    "14.01 to_gym_east_room from CinnabarIsland_PokemonCenter_1F (7, 4) facing north to CinnabarIsland_Gym (25, 16) facing north",
    "14.01   walk south 5 (warp) -> CinnabarIsland (14, 12) [walk only]",
    "14.01   walk east 7 -> CinnabarIsland (21, 12)",
    "14.01   walk north 7 (to wall) -> CinnabarIsland (21, 5)",
    "14.01   walk west 1 -> CinnabarIsland (20, 5)",
    "14.01   walk north 1 (warp) -> CinnabarIsland_Gym (25, 23)",
    "14.01   walk north 7 (to wall) -> CinnabarIsland_Gym (25, 16) [walk only]",
    "14.02b below_gym_door from CinnabarIsland_Gym (26, 10) facing south to CinnabarIsland (20, 5) facing south",
    "14.02b   walk south 13 (to wall) -> CinnabarIsland_Gym (26, 23) [walk only]",
    "14.02b   walk west 1 -> CinnabarIsland_Gym (25, 23) [walk only] [tapped]",
    "14.02b   walk south 1 (warp) -> CinnabarIsland (20, 5) [walk only]",
    "14.02b below_center_door from CinnabarIsland (20, 5) facing south to CinnabarIsland (14, 12) facing north",
    "14.02b   walk east 1 (to wall) -> CinnabarIsland (21, 5)",
    "14.02b   walk south 7 -> CinnabarIsland (21, 12)",
    "14.02b   walk west 7 -> CinnabarIsland (14, 12)",
    "14.02b   face north -> CinnabarIsland (14, 12)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
