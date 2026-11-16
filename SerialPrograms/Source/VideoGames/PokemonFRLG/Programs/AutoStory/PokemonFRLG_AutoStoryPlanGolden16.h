/*  FRLG AutoStory Plan Golden, Segment 16
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines from segment 16 on, after
 *  PokemonFRLG_AutoStoryPlanGolden15.h's. The test reads FRLG_PLAN_GOLDEN,
 *  FRLG_PLAN_GOLDEN_10, _11, _12 and _15, then these.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden16_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden16_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines from segment 16 on, after FRLG_PLAN_GOLDEN_15's.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_16[] = {
    "16.01 silph_3f_by_pad from SaffronCity_PokemonCenter_1F (7, 4) facing north to SilphCo_3F (5, 6) facing west",
    "16.01   walk south 5 (warp) -> SaffronCity (24, 39) [walk only]",
    "16.01   walk west 7 -> SaffronCity (17, 39)",
    "16.01   walk north 5 -> SaffronCity (17, 34)",
    "16.01   walk east 16 -> SaffronCity (33, 34)",
    "16.01   walk north 4 (warp) -> SilphCo_1F (8, 20)",
    "16.01   walk east 14 (to wall) -> SilphCo_1F (22, 20)",
    "16.01   walk north 4 (to wall) -> SilphCo_1F (22, 16)",
    "16.01   walk east 7 (to wall) -> SilphCo_1F (29, 16)",
    "16.01   walk north 12 (to wall) -> SilphCo_1F (29, 4)",
    "16.01   walk east 1 -> SilphCo_1F (30, 4)",
    "16.01   walk north 2 (to wall) -> SilphCo_1F (30, 2)",
    "16.01   walk east 2 (warp) -> SilphCo_2F (30, 2)",
    "16.01   walk west 3 (warp) -> SilphCo_3F (28, 2)",
    "16.01   walk south 4 -> SilphCo_3F (28, 6)",
    "16.01   walk west 23 -> SilphCo_3F (5, 6)",
    "16.02 to_corridor from SilphCo_5F (15, 7) facing west to SilphCo_5F (12, 20) facing east",
    "16.02   walk south 1 (to wall) -> SilphCo_5F (15, 8)",
    "16.02   walk west 5 -> SilphCo_5F (10, 8)",
    "16.02   walk south 12 (warp) -> SilphCo_9F (22, 18)",
    "16.02   walk west 4 (to wall) -> SilphCo_9F (18, 18)",
    "16.02   walk east 4 (warp) -> SilphCo_5F (10, 20)",
    "16.02   walk east 2 (to wall) -> SilphCo_5F (12, 20)",
    "16.03 to_3f_trainer from SilphCo_5F (21, 21) facing east to SilphCo_3F (22, 9) facing south",
    "16.03   walk west 9 (to wall) -> SilphCo_5F (12, 21)",
    "16.03   walk north 1 (to wall) -> SilphCo_5F (12, 20)",
    "16.03   walk west 2 (warp) -> SilphCo_9F (22, 18)",
    "16.03   walk west 4 (to wall) -> SilphCo_9F (18, 18)",
    "16.03   walk east 4 (warp) -> SilphCo_5F (10, 20)",
    "16.03   walk north 12 -> SilphCo_5F (10, 8)",
    "16.03   walk east 5 -> SilphCo_5F (15, 8)",
    "16.03   walk north 1 (warp) -> SilphCo_3F (4, 6)",
    "16.03   walk south 1 (to wall) -> SilphCo_3F (4, 7)",
    "16.03   walk east 18 -> SilphCo_3F (22, 7)",
    "16.03   walk south 2 -> SilphCo_3F (22, 9)",
    "16.03 to_3f_door from SilphCo_3F (22, 9) facing south to SilphCo_3F (18, 13) facing west",
    "16.03   walk south 4 -> SilphCo_3F (22, 13)",
    "16.03   open gate west -> SilphCo_3F (22, 13)",
    "16.03   walk west 4 (to wall) -> SilphCo_3F (18, 13)",
    "16.03 to_3f_pad_row from SilphCo_3F (18, 13) facing west to SilphCo_3F (18, 14) facing south",
    "16.03   walk south 1 -> SilphCo_3F (18, 14)",
    "16.03 to_rival_trigger from SilphCo_3F (18, 14) facing south to SilphCo_7F (3, 4) facing west",
    "16.03   walk west 5 (warp) -> SilphCo_7F (5, 4)",
    "16.03   walk west 2 -> SilphCo_7F (3, 4)",
    "16.03b to_lapras_guy from SilphCo_7F (2, 4) facing south to SilphCo_7F (1, 7) facing west",
    "16.03b   walk south 1 (to wall) -> SilphCo_7F (2, 5)",
    "16.03b   walk west 2 (to wall) -> SilphCo_7F (0, 5)",
    "16.03b   walk south 1 (to wall) -> SilphCo_7F (0, 6)",
    "16.03b   walk east 1 (to wall) -> SilphCo_7F (1, 6)",
    "16.03b   walk south 1 -> SilphCo_7F (1, 7)",
    "16.03b   face west -> SilphCo_7F (1, 7)",
    "16.05 to_left_wall from SilphCo_7F (1, 7) facing west to SilphCo_11F (0, 18) facing south",
    "16.05   walk east 4 (to wall) -> SilphCo_7F (5, 7)",
    "16.05   walk south 1 (warp) -> SilphCo_11F (2, 5)",
    "16.05   walk west 2 (to wall) -> SilphCo_11F (0, 5)",
    "16.05   walk south 13 -> SilphCo_11F (0, 18)",
    "16.05 to_rocket_column from SilphCo_11F (0, 18) facing south to SilphCo_11F (2, 18) facing east",
    "16.05   walk east 2 -> SilphCo_11F (2, 18)",
    "16.05 to_giovanni_door from SilphCo_11F (2, 18) facing east to SilphCo_11F (5, 16) facing north",
    "16.05   walk east 3 -> SilphCo_11F (5, 18)",
    "16.05   open gate north -> SilphCo_11F (5, 18)",
    "16.05   walk north 2 -> SilphCo_11F (5, 16)",
    "16.05b below_president from SilphCo_11F (5, 15) facing east to SilphCo_11F (9, 10) facing north",
    "16.05b   walk north 7 (to wall) -> SilphCo_11F (5, 8)",
    "16.05b   walk south 1 -> SilphCo_11F (5, 9)",
    "16.05b   walk east 3 (to wall) -> SilphCo_11F (8, 9)",
    "16.05b   walk south 1 (to wall) -> SilphCo_11F (8, 10)",
    "16.05b   walk east 1 -> SilphCo_11F (9, 10)",
    "16.05b   face north -> SilphCo_11F (9, 10)",
    "16.06 below_center_door from SaffronCity (24, 39) facing south to SaffronCity (24, 39) facing north",
    "16.06   face north -> SaffronCity (24, 39)",
    "16.06b below_center_row_40 from SaffronCity_PokemonCenter_1F (7, 4) facing north to SaffronCity (24, 40) facing south",
    "16.06b   walk south 5 (warp) -> SaffronCity (24, 39) [walk only]",
    "16.06b   walk south 1 -> SaffronCity (24, 40)",
    "16.06b east_wall_row_40 from SaffronCity (24, 40) facing south to SaffronCity (52, 40) facing east",
    "16.06b   walk east 28 (to wall) -> SaffronCity (52, 40)",
    "16.06b below_row_14_wall from SaffronCity (52, 40) facing east to SaffronCity (52, 15) facing north",
    "16.06b   walk north 25 (to wall) -> SaffronCity (52, 15)",
    "16.06b below_gym_door from SaffronCity (52, 15) facing north to SaffronCity (46, 15) facing west",
    "16.06b   walk west 6 -> SaffronCity (46, 15)",
    "16.06b into_gym from SaffronCity (46, 15) facing west to SaffronCity_Gym (14, 23) facing north",
    "16.06b   walk north 3 (warp) -> SaffronCity_Gym (14, 23)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
