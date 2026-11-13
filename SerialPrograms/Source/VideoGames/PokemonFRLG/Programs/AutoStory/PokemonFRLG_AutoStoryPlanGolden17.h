/*  FRLG AutoStory Plan Golden, Segment 17
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 17.01 (after PokemonFRLG_AutoStoryPlanGolden16.h's,
 *  before PokemonFRLG_AutoStoryPlanGolden13.h's) and for 17.02 to 17.04
 *  (after PokemonFRLG_AutoStoryPlanGolden14.h's), in the story's order.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden17_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden17_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines of 17.01, Saffron to Cinnabar.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_17_SURF[] = {
    "17.01 saffron_outside from SaffronCity_PokemonCenter_1F (7, 4) facing north to SaffronCity (24, 39) facing south",
    "17.01   walk south 5 (warp) -> SaffronCity (24, 39) [walk only]",
    "17.01 pallet_shore from PalletTown (6, 8) facing south to PalletTown (8, 16) facing south",
    "17.01   walk south 2 (to wall) -> PalletTown (6, 10)",
    "17.01   walk east 4 -> PalletTown (10, 10)",
    "17.01   walk south 6 (to wall) -> PalletTown (10, 16)",
    "17.01   walk west 2 -> PalletTown (8, 16)",
    "17.01   face south -> PalletTown (8, 16)",
    "17.01 surf_route_21 from PalletTown (8, 17) facing south to CinnabarIsland (14, 12) facing north",
    "17.01   walk south 29 -> Route21_North (8, 26) [surfing] [wild] [connection 3]",
    "17.01   walk south 1 -> Route21_North (8, 27) [surfing] [wild]",
    "17.01   surf north -> Route21_North (8, 26) [wild]",
    "17.01   walk east 5 -> Route21_North (13, 26) [surfing] [wild]",
    "17.01   walk east 1 -> Route21_North (14, 26) [surfing] [wild]",
    "17.01   surf west -> Route21_North (13, 26) [wild]",
    "17.01   walk south 68 -> Route21_South (13, 44) [surfing] [wild] [connection 24]",
    "17.01   walk south 1 -> Route21_South (13, 45) [surfing] [wild]",
    "17.01   walk south 10 -> CinnabarIsland (13, 5) [connection 5]",
    "17.01   walk east 8 (to wall) -> CinnabarIsland (21, 5)",
    "17.01   walk south 7 -> CinnabarIsland (21, 12)",
    "17.01   walk west 7 -> CinnabarIsland (14, 12)",
    "17.01   face north -> CinnabarIsland (14, 12)",
};

//  The stored plan lines of 17.02 to 17.04, Cinnabar to Viridian Gym.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_17_GYM[] = {
    "17.02 cinnabar_outside from CinnabarIsland_PokemonCenter_1F (7, 4) facing north to CinnabarIsland (14, 12) facing south",
    "17.02   walk south 5 (warp) -> CinnabarIsland (14, 12) [walk only]",
    "17.02 below_gym_door from ViridianCity (26, 27) facing south to ViridianCity (36, 11) facing north",
    "17.02   walk west 4 -> ViridianCity (22, 27)",
    "17.02   walk north 21 -> ViridianCity (22, 6)",
    "17.02   walk east 19 (to wall) -> ViridianCity (41, 6)",
    "17.02   walk south 5 -> ViridianCity (41, 11)",
    "17.02   walk west 5 -> ViridianCity (36, 11)",
    "17.02   face north -> ViridianCity (36, 11)",
    "17.02 below_warren_line from ViridianCity_Gym (17, 22) facing north to ViridianCity_Gym (15, 8) facing north",
    "17.02   walk north 7 (to wall) -> ViridianCity_Gym (17, 15) [walk only]",
    "17.02   walk west 3 (to wall) -> ViridianCity_Gym (14, 15) [walk only]",
    "17.02   walk north 2 (to wall) -> ViridianCity_Gym (14, 13) [walk only]",
    "17.02   walk west 1 -> ViridianCity_Gym (13, 13) [walk only]",
    "17.02   walk north 3 (to wall) -> ViridianCity_Gym (13, 10) [walk only]",
    "17.02   walk east 3 (to wall) -> ViridianCity_Gym (16, 10) [walk only]",
    "17.02   walk north 2 -> ViridianCity_Gym (16, 8) [walk only]",
    "17.02   walk west 1 (to wall) -> ViridianCity_Gym (15, 8) [walk only]",
    "17.02   face north -> ViridianCity_Gym (15, 8) [walk only]",
    "17.03 below_takashi_line from ViridianCity_Gym (15, 7) facing north to ViridianCity_Gym (10, 6) facing north",
    "17.03   walk north 1 (to wall) -> ViridianCity_Gym (15, 6) [walk only]",
    "17.03   walk west 3 (to wall) -> ViridianCity_Gym (12, 6) [walk only]",
    "17.03   walk south 1 (to wall) -> ViridianCity_Gym (12, 7) [walk only]",
    "17.03   walk west 2 (to wall) -> ViridianCity_Gym (10, 7) [walk only]",
    "17.03   walk north 1 -> ViridianCity_Gym (10, 6) [walk only]",
    "17.03 below_giovanni from ViridianCity_Gym (10, 5) facing north to ViridianCity_Gym (2, 3) facing north",
    "17.03   walk north 2 -> ViridianCity_Gym (10, 3) [walk only]",
    "17.03   walk west 3 -> ViridianCity_Gym (7, 3) [walk only]",
    "17.03   walk south 3 (to wall) -> ViridianCity_Gym (7, 6) [walk only]",
    "17.03   walk west 5 -> ViridianCity_Gym (2, 6) [walk only]",
    "17.03   walk north 3 -> ViridianCity_Gym (2, 3) [walk only]",
    "17.04 gym_exit from ViridianCity_Gym (2, 3) facing north to ViridianCity (36, 11) facing south",
    "17.04   walk south 4 (to wall) -> ViridianCity_Gym (2, 7) [walk only]",
    "17.04   walk east 4 (to wall) -> ViridianCity_Gym (6, 7) [walk only]",
    "17.04   walk north 1 (to wall) -> ViridianCity_Gym (6, 6) [walk only]",
    "17.04   walk east 1 (to wall) -> ViridianCity_Gym (7, 6) [walk only]",
    "17.04   walk north 3 -> ViridianCity_Gym (7, 3) [walk only]",
    "17.04   walk east 4 (spins 6) -> ViridianCity_Gym (17, 3) [walk only]",
    "17.04   walk east 1 (spins 11) -> ViridianCity_Gym (18, 14) [walk only]",
    "17.04   walk south 8 (to wall) -> ViridianCity_Gym (18, 22) [walk only]",
    "17.04   walk west 1 -> ViridianCity_Gym (17, 22) [walk only] [tapped]",
    "17.04   walk south 1 (warp) -> ViridianCity (36, 11) [walk only]",
    "17.04 below_center_door from ViridianCity (36, 11) facing south to ViridianCity (26, 27) facing north",
    "17.04   walk east 5 (to wall) -> ViridianCity (41, 11)",
    "17.04   walk north 7 (to wall) -> ViridianCity (41, 4)",
    "17.04   walk west 13 (to wall) -> ViridianCity (28, 4)",
    "17.04   walk south 4 (to wall) -> ViridianCity (28, 8)",
    "17.04   walk west 6 -> ViridianCity (22, 8)",
    "17.04   walk south 19 -> ViridianCity (22, 27)",
    "17.04   walk east 4 -> ViridianCity (26, 27)",
    "17.04   face north -> ViridianCity (26, 27)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
