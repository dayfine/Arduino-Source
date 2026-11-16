/*  FRLG AutoStory Plan Golden, Segment 18
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 18.01 to 18.03, Saffron Gym and the X items
 *  trip to Celadon (after
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

//  The stored plan lines of 18.01 to 18.03, Saffron Gym and the X items
//  trip to Celadon.
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
    "18.02b below_center_door from SaffronCity (24, 39) facing south to SaffronCity (24, 39) facing north",
    "18.02b   face north -> SaffronCity (24, 39)",
    "18.03 saffron_outside from SaffronCity_PokemonCenter_1F (7, 4) facing north to SaffronCity (24, 39) facing south",
    "18.03   walk south 5 (warp) -> SaffronCity (24, 39) [walk only]",
    "18.03 below_store_lift from CeladonCity (48, 12) facing south to CeladonCity_DepartmentStore_1F (6, 2) facing north",
    "18.03   walk west 25 -> CeladonCity (23, 12)",
    "18.03   walk south 4 (to wall) -> CeladonCity (23, 16)",
    "18.03   walk west 8 -> CeladonCity (15, 16)",
    "18.03   walk north 2 (warp) -> CeladonCity_DepartmentStore_1F (10, 14)",
    "18.03   walk north 9 (to wall) -> CeladonCity_DepartmentStore_1F (10, 5) [walk only]",
    "18.03   walk west 4 -> CeladonCity_DepartmentStore_1F (6, 5) [walk only]",
    "18.03   walk north 3 -> CeladonCity_DepartmentStore_1F (6, 2) [walk only]",
    "18.03 x_items_clerk from CeladonCity_DepartmentStore_5F (6, 2) facing south to CeladonCity_DepartmentStore_5F (3, 7) facing west",
    "18.03   walk south 4 -> CeladonCity_DepartmentStore_5F (6, 6) [walk only]",
    "18.03   walk west 3 (to wall) -> CeladonCity_DepartmentStore_5F (3, 6) [walk only]",
    "18.03   walk south 1 -> CeladonCity_DepartmentStore_5F (3, 7) [walk only]",
    "18.03   face west -> CeladonCity_DepartmentStore_5F (3, 7) [walk only]",
    "18.03 below_5f_lift from CeladonCity_DepartmentStore_5F (3, 7) facing west to CeladonCity_DepartmentStore_5F (6, 2) facing north",
    "18.03   walk north 5 (to wall) -> CeladonCity_DepartmentStore_5F (3, 2) [walk only]",
    "18.03   walk east 3 -> CeladonCity_DepartmentStore_5F (6, 2) [walk only]",
    "18.03   face north -> CeladonCity_DepartmentStore_5F (6, 2) [walk only]",
    "18.03 out_of_store from CeladonCity_DepartmentStore_1F (6, 2) facing south to CeladonCity (11, 15) facing south",
    "18.03   walk south 5 (to wall) -> CeladonCity_DepartmentStore_1F (6, 7) [walk only]",
    "18.03   walk west 4 -> CeladonCity_DepartmentStore_1F (2, 7) [walk only]",
    "18.03   walk south 8 (warp) -> CeladonCity (11, 15) [walk only]",
    "18.03 below_center_door from SaffronCity (24, 39) facing south to SaffronCity (24, 39) facing north",
    "18.03   face north -> SaffronCity (24, 39)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
