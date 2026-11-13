/*  FRLG AutoStory Plan Golden, Segment 15
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines from segment 15 on, after
 *  PokemonFRLG_AutoStoryPlanGolden14.h's. The test reads FRLG_PLAN_GOLDEN,
 *  FRLG_PLAN_GOLDEN_10, FRLG_PLAN_GOLDEN_11, FRLG_PLAN_GOLDEN_12, FRLG_PLAN_GOLDEN_14, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden15_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden15_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines from segment 15 on, after FRLG_PLAN_GOLDEN_14's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_15[] = {
    "15.01 west_of_tea_guard from CeladonCity_PokemonCenter_1F (7, 4) facing north to Route7_EastEntrance (5, 4) facing east",
    "15.01   walk south 5 (warp) -> CeladonCity (48, 12) [walk only]",
    "15.01   walk east 19 -> Route7 (7, 2) [connection 12]",
    "15.01   walk south 8 -> Route7 (7, 10)",
    "15.01   walk east 9 (warp) -> Route7_EastEntrance (1, 5)",
    "15.01   walk east 4 -> Route7_EastEntrance (5, 5)",
    "15.01   walk north 1 (to wall) -> Route7_EastEntrance (5, 4)",
    "15.01   face east -> Route7_EastEntrance (5, 4)",
    "15.01 below_center_door from Route7_EastEntrance (6, 4) facing north to SaffronCity (24, 39) facing north",
    "15.01   walk south 1 -> Route7_EastEntrance (6, 5)",
    "15.01   walk east 6 (warp) -> SaffronCity (8, 27)",
    "15.01   walk east 9 -> SaffronCity (17, 27)",
    "15.01   walk south 12 -> SaffronCity (17, 39)",
    "15.01   walk east 7 -> SaffronCity (24, 39)",
    "15.01   face north -> SaffronCity (24, 39)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
