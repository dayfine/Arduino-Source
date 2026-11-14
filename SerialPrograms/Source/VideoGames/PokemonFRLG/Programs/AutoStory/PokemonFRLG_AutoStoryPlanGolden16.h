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
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
