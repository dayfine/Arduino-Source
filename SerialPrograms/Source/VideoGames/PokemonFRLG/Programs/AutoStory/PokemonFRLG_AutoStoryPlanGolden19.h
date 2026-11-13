/*  FRLG AutoStory Plan Golden, Segment 19
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 19.01 and after (Route 22's rival, Route 23,
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

//  The stored plan lines of 19.01 and after.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_19[] = {
    "19.01 viridian_mart_clerk from ViridianCity_PokemonCenter_1F (7, 4) facing north to ViridianCity_Mart (4, 3) facing west",
    "19.01   walk south 5 (warp) -> ViridianCity (26, 27) [walk only]",
    "19.01   walk south 2 -> ViridianCity (26, 29)",
    "19.01   walk east 10 -> ViridianCity (36, 29)",
    "19.01   walk north 10 (warp) -> ViridianCity_Mart (4, 7)",
    "19.01   walk north 4 -> ViridianCity_Mart (4, 3) [walk only]",
    "19.01   face west -> ViridianCity_Mart (4, 3) [walk only]",
    "19.01 route22_east_of_trigger from ViridianCity_Mart (4, 3) facing west to Route22 (34, 5) facing west",
    "19.01   walk south 5 (warp) -> ViridianCity (36, 20) [walk only]",
    "19.01   walk west 13 -> ViridianCity (23, 20)",
    "19.01   walk north 1 (to wall) -> ViridianCity (23, 19)",
    "19.01   walk west 29 (to wall) -> Route22 (42, 9) [connection 24]",
    "19.01   walk south 5 -> Route22 (42, 14)",
    "19.01   walk west 6 -> Route22 (36, 14)",
    "19.01   walk north 9 -> Route22 (36, 5) [wild]",
    "19.01   walk west 2 -> Route22 (34, 5)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
