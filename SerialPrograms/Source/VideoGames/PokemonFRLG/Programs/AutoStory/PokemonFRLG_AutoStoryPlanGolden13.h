/*  FRLG AutoStory Plan Golden, Segment 13
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines of segment 13 (the Pokemon Mansion), which STORY_ORDER
 *  places after segment 16's. The test reads FRLG_PLAN_GOLDEN_16, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden13_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden13_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines of segment 13, after FRLG_PLAN_GOLDEN_16's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_13[] = {
    "13.01 mansion_3f from CinnabarIsland_PokemonCenter_1F (7, 4) facing north to PokemonMansion_3F (8, 3) facing east",
    "13.01   walk south 5 (warp) -> CinnabarIsland (14, 12) [walk only]",
    "13.01   walk east 7 -> CinnabarIsland (21, 12)",
    "13.01   walk north 7 (to wall) -> CinnabarIsland (21, 5)",
    "13.01   walk west 13 -> CinnabarIsland (8, 5)",
    "13.01   walk north 2 (warp) -> PokemonMansion_1F (8, 33)",
    "13.01   walk north 20 (to wall) -> PokemonMansion_1F (8, 13) [wild]",
    "13.01   walk east 3 (warp) -> PokemonMansion_2F (6, 14) [wild]",
    "13.01   walk south 4 (to wall) -> PokemonMansion_2F (6, 18) [wild]",
    "13.01   walk east 11 (to wall) -> PokemonMansion_2F (17, 18) [wild]",
    "13.01   walk north 12 -> PokemonMansion_2F (17, 6) [wild]",
    "13.01   walk west 8 -> PokemonMansion_2F (9, 6) [wild]",
    "13.01   walk north 3 (to wall) -> PokemonMansion_2F (9, 3) [wild]",
    "13.01   walk east 1 (warp) -> PokemonMansion_3F (8, 3)",
    "13.02 down_to_b1f from PokemonMansion_1F (26, 27) facing north to PokemonMansion_B1F (34, 29) facing west",
    "13.02   walk west 2 (warp) -> PokemonMansion_B1F (34, 29)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
