/*  FRLG AutoStory Plan Golden, Segment 18
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 18.01 to 18.02b, Saffron Gym (after
 *  PokemonFRLG_AutoStoryPlanGolden16.h's, before PokemonFRLG_AutoStoryPlanGolden17.h's
 *  17.01), in the story's order.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden18_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden18_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines of 18.01 to 18.02b, Saffron Gym.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_18[] = {
    "18.01 by_pad_to_bottom_right from SaffronCity_Gym (14, 23) facing north to SaffronCity_Gym (17, 20) facing east",
    "18.01   walk east 3 -> SaffronCity_Gym (17, 23) [walk only]",
    "18.01   walk north 3 -> SaffronCity_Gym (17, 20) [walk only]",
    "18.01   face east -> SaffronCity_Gym (17, 20) [walk only]",
    "18.01 along_row_22 from SaffronCity_Gym (28, 23) facing east to SaffronCity_Gym (20, 22) facing west",
    "18.01   walk north 1 -> SaffronCity_Gym (28, 22) [walk only]",
    "18.01   walk west 8 (to wall) -> SaffronCity_Gym (20, 22) [walk only]",
    "18.01 below_pad_to_top_right from SaffronCity_Gym (20, 22) facing west to SaffronCity_Gym (20, 21) facing north",
    "18.01   walk north 1 -> SaffronCity_Gym (20, 21) [walk only]",
    "18.01 by_pad_to_top_left from SaffronCity_Gym (28, 4) facing north to SaffronCity_Gym (20, 6) facing south",
    "18.01   walk south 2 -> SaffronCity_Gym (28, 6) [walk only]",
    "18.01   walk west 8 (to wall) -> SaffronCity_Gym (20, 6) [walk only]",
    "18.01   face south -> SaffronCity_Gym (20, 6) [walk only]",
    "18.01 by_pad_to_sabrina from SaffronCity_Gym (0, 4) facing south to SaffronCity_Gym (0, 6) facing south",
    "18.01   walk south 2 -> SaffronCity_Gym (0, 6) [walk only]",
    "18.01 below_sabrina from SaffronCity_Gym (18, 15) facing south to SaffronCity_Gym (14, 12) facing north",
    "18.01   walk west 4 -> SaffronCity_Gym (14, 15) [walk only]",
    "18.01   walk north 3 -> SaffronCity_Gym (14, 12) [walk only]",
    "18.02b by_pad_out_of_sabrina from SaffronCity_Gym (14, 12) facing north to SaffronCity_Gym (17, 15) facing east",
    "18.02b   walk south 3 (to wall) -> SaffronCity_Gym (14, 15) [walk only]",
    "18.02b   walk east 3 -> SaffronCity_Gym (17, 15) [walk only]",
    "18.02b by_pad_from_top_left from SaffronCity_Gym (0, 7) facing east to SaffronCity_Gym (0, 5) facing north",
    "18.02b   walk north 2 -> SaffronCity_Gym (0, 5) [walk only]",
    "18.02b along_row_6 from SaffronCity_Gym (20, 7) facing north to SaffronCity_Gym (28, 6) facing east",
    "18.02b   walk north 1 -> SaffronCity_Gym (20, 6) [walk only]",
    "18.02b   walk east 8 (to wall) -> SaffronCity_Gym (28, 6) [walk only]",
    "18.02b below_pad_from_top_right from SaffronCity_Gym (28, 6) facing east to SaffronCity_Gym (28, 5) facing north",
    "18.02b   walk north 1 -> SaffronCity_Gym (28, 5) [walk only]",
    "18.02b by_pad_from_bottom_right from SaffronCity_Gym (20, 20) facing north to SaffronCity_Gym (28, 22) facing south",
    "18.02b   walk south 2 -> SaffronCity_Gym (20, 22) [walk only]",
    "18.02b   walk east 8 (to wall) -> SaffronCity_Gym (28, 22) [walk only]",
    "18.02b   face south -> SaffronCity_Gym (28, 22) [walk only]",
    "18.02b below_center_door from SaffronCity_Gym (18, 20) facing south to SaffronCity (24, 39) facing north",
    "18.02b   walk south 3 (to wall) -> SaffronCity_Gym (18, 23) [walk only]",
    "18.02b   walk west 4 -> SaffronCity_Gym (14, 23) [walk only] [tapped]",
    "18.02b   walk south 1 (warp) -> SaffronCity (46, 13) [walk only]",
    "18.02b   walk south 4 (to wall) -> SaffronCity (46, 17)",
    "18.02b   walk east 4 -> SaffronCity (50, 17)",
    "18.02b   walk south 24 (to wall) -> SaffronCity (50, 41)",
    "18.02b   walk west 26 -> SaffronCity (24, 41)",
    "18.02b   walk north 2 -> SaffronCity (24, 39)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
