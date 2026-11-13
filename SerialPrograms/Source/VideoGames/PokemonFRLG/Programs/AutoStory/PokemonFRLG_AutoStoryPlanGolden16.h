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
    "16.01   walk west 9 (to wall) -> SaffronCity (15, 39)",
    "16.01   walk north 7 -> SaffronCity (15, 32)",
    "16.01   walk east 18 -> SaffronCity (33, 32)",
    "16.01   walk north 2 (warp) -> SilphCo_1F (8, 20)",
    "16.01   walk north 15 (to wall) -> SilphCo_1F (8, 5)",
    "16.01   walk east 23 -> SilphCo_1F (31, 5)",
    "16.01   walk north 3 (to wall) -> SilphCo_1F (31, 2)",
    "16.01   walk east 1 (warp) -> SilphCo_2F (30, 2)",
    "16.01   walk west 3 (warp) -> SilphCo_3F (28, 2)",
    "16.01   walk south 4 -> SilphCo_3F (28, 6)",
    "16.01   walk west 23 -> SilphCo_3F (5, 6)",
    "16.02 to_corridor from SilphCo_5F (15, 7) facing west to SilphCo_5F (12, 21) facing south",
    "16.02   walk south 1 (to wall) -> SilphCo_5F (15, 8)",
    "16.02   walk west 5 -> SilphCo_5F (10, 8)",
    "16.02   walk south 12 (warp) -> SilphCo_9F (22, 18)",
    "16.02   walk west 4 (to wall) -> SilphCo_9F (18, 18)",
    "16.02   walk east 4 (warp) -> SilphCo_5F (10, 20)",
    "16.02   walk east 2 (to wall) -> SilphCo_5F (12, 20)",
    "16.02   walk south 1 (to wall) -> SilphCo_5F (12, 21)",
    "16.03 to_3f_trainer from SilphCo_5F (21, 21) facing east to SilphCo_3F (22, 9) facing south",
    "16.03   walk west 11 -> SilphCo_5F (10, 21)",
    "16.03   walk north 1 (warp) -> SilphCo_9F (22, 18)",
    "16.03   walk west 4 (to wall) -> SilphCo_9F (18, 18)",
    "16.03   walk east 4 (warp) -> SilphCo_5F (10, 20)",
    "16.03   walk north 12 -> SilphCo_5F (10, 8)",
    "16.03   walk east 5 -> SilphCo_5F (15, 8)",
    "16.03   walk north 1 (warp) -> SilphCo_3F (4, 6)",
    "16.03   walk east 18 -> SilphCo_3F (22, 6)",
    "16.03   walk south 3 -> SilphCo_3F (22, 9)",
    "16.03 to_3f_door from SilphCo_3F (22, 9) facing south to SilphCo_3F (18, 13) facing west",
    "16.03   walk south 4 -> SilphCo_3F (22, 13)",
    "16.03   open gate west -> SilphCo_3F (22, 13)",
    "16.03   walk west 4 (to wall) -> SilphCo_3F (18, 13)",
    "16.03 to_3f_pad_row from SilphCo_3F (18, 13) facing west to SilphCo_3F (18, 14) facing south",
    "16.03   walk south 1 -> SilphCo_3F (18, 14)",
    "16.03 to_rival_trigger from SilphCo_3F (18, 14) facing south to SilphCo_7F (3, 4) facing west",
    "16.03   walk west 5 (warp) -> SilphCo_7F (5, 4)",
    "16.03   walk west 2 -> SilphCo_7F (3, 4)",
    "16.03 to_lapras_guy from SilphCo_7F (2, 4) facing south to SilphCo_7F (1, 7) facing west",
    "16.03   walk west 1 -> SilphCo_7F (1, 4)",
    "16.03   walk south 3 -> SilphCo_7F (1, 7)",
    "16.03   face west -> SilphCo_7F (1, 7)",
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
    "16.05 below_president from SilphCo_11F (5, 15) facing north to SilphCo_11F (9, 10) facing north",
    "16.05   walk north 5 -> SilphCo_11F (5, 10)",
    "16.05   walk east 4 -> SilphCo_11F (9, 10)",
    "16.05   face north -> SilphCo_11F (9, 10)",
    "16.06 below_center_door from SilphCo_11F (9, 10) facing north to SaffronCity (24, 39) facing north",
    "16.06   walk west 4 (to wall) -> SilphCo_11F (5, 10)",
    "16.06   walk south 9 (to wall) -> SilphCo_11F (5, 19)",
    "16.06   walk west 3 -> SilphCo_11F (2, 19)",
    "16.06   walk north 14 (warp) -> SilphCo_7F (5, 8)",
    "16.06   walk north 1 (to wall) -> SilphCo_7F (5, 7)",
    "16.06   walk west 4 -> SilphCo_7F (1, 7)",
    "16.06   walk north 3 (to wall) -> SilphCo_7F (1, 4)",
    "16.06   walk east 4 (warp) -> SilphCo_3F (13, 14)",
    "16.06   walk east 6 (to wall) -> SilphCo_3F (19, 14)",
    "16.06   walk north 2 -> SilphCo_3F (19, 12)",
    "16.06   walk east 6 -> SilphCo_3F (25, 12)",
    "16.06   walk north 8 (to wall) -> SilphCo_3F (25, 4)",
    "16.06   walk east 3 -> SilphCo_3F (28, 4)",
    "16.06   walk north 2 (to wall) -> SilphCo_3F (28, 2)",
    "16.06   walk west 1 (warp) -> SilphCo_2F (28, 2)",
    "16.06   walk east 3 (warp) -> SilphCo_1F (31, 2)",
    "16.06   walk west 1 (to wall) -> SilphCo_1F (30, 2)",
    "16.06   walk south 5 (to wall) -> SilphCo_1F (30, 7)",
    "16.06   walk west 22 -> SilphCo_1F (8, 7)",
    "16.06   walk south 14 (warp) -> SaffronCity (33, 31)",
    "16.06   walk south 3 (to wall) -> SaffronCity (33, 34)",
    "16.06   walk west 18 (to wall) -> SaffronCity (15, 34)",
    "16.06   walk south 5 -> SaffronCity (15, 39)",
    "16.06   walk east 9 -> SaffronCity (24, 39)",
    "16.06   face north -> SaffronCity (24, 39)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
