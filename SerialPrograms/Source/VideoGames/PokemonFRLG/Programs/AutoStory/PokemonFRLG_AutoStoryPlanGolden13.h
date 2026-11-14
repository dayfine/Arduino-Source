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
    "13.01 mansion_holes from CinnabarIsland (8, 4) facing north to PokemonMansion_3F (19, 13) facing south",
    "13.01   walk north 1 (warp) -> PokemonMansion_1F (8, 33)",
    "13.01   walk north 20 (to wall) -> PokemonMansion_1F (8, 13) [wild]",
    "13.01   walk east 3 (warp) -> PokemonMansion_2F (6, 14) [wild]",
    "13.01   walk south 4 (to wall) -> PokemonMansion_2F (6, 18) [wild]",
    "13.01   walk east 11 (to wall) -> PokemonMansion_2F (17, 18) [wild]",
    "13.01   walk north 12 -> PokemonMansion_2F (17, 6) [wild]",
    "13.01   walk west 8 -> PokemonMansion_2F (9, 6) [wild]",
    "13.01   walk north 3 (to wall) -> PokemonMansion_2F (9, 3) [wild]",
    "13.01   walk east 1 (warp) -> PokemonMansion_3F (8, 3)",
    "13.01   walk south 3 -> PokemonMansion_3F (8, 6) [tapped] [wild]",
    "13.01   walk east 4 -> PokemonMansion_3F (12, 6) [tapped] [wild]",
    "13.01   press switch north -> PokemonMansion_3F (12, 6)",
    "13.01   walk east 8 (to wall) -> PokemonMansion_3F (20, 6) [wild]",
    "13.01   walk north 3 (to wall) -> PokemonMansion_3F (20, 3) [wild]",
    "13.01   walk west 1 (to wall) -> PokemonMansion_3F (19, 3) [wild]",
    "13.01   walk south 10 -> PokemonMansion_3F (19, 13) [wild]",
    "13.02 to_ted_room from PokemonMansion_3F (19, 13) facing south to PokemonMansion_1F (17, 23) facing south",
    "13.02   walk south 5 (warp) -> PokemonMansion_1F (20, 22) [wild]",
    "13.02   walk west 3 (to wall) -> PokemonMansion_1F (17, 22) [wild]",
    "13.02   walk south 1 -> PokemonMansion_1F (17, 23) [wild]",
    "13.02 past_ted from PokemonMansion_1F (17, 23) facing south to PokemonMansion_1F (17, 25) facing south",
    "13.02   walk south 2 -> PokemonMansion_1F (17, 25) [wild]",
    "13.02 down_to_b1f from PokemonMansion_1F (17, 25) facing south to PokemonMansion_B1F (34, 29) facing west",
    "13.02   walk south 8 (to wall) -> PokemonMansion_1F (17, 33) [wild]",
    "13.02   walk east 13 (to wall) -> PokemonMansion_1F (30, 33) [wild]",
    "13.02   walk north 6 (to wall) -> PokemonMansion_1F (30, 27) [wild]",
    "13.02   walk west 6 (warp) -> PokemonMansion_B1F (34, 29) [wild]",
    "13.03 to_elixir from PokemonMansion_B1F (34, 29) facing west to PokemonMansion_B1F (34, 5) facing east",
    "13.03   walk west 7 (to wall) -> PokemonMansion_B1F (27, 29) [wild]",
    "13.03   walk north 12 (to wall) -> PokemonMansion_B1F (27, 17) [wild]",
    "13.03   walk west 5 -> PokemonMansion_B1F (22, 17) [wild]",
    "13.03   walk south 9 (to wall) -> PokemonMansion_B1F (22, 26) [wild]",
    "13.03   walk east 3 (to wall) -> PokemonMansion_B1F (25, 26) [wild]",
    "13.03   walk south 5 (to wall) -> PokemonMansion_B1F (25, 31) [wild]",
    "13.03   walk west 1 -> PokemonMansion_B1F (24, 31) [wild]",
    "13.03   walk north 1 (to wall) -> PokemonMansion_B1F (24, 30) [wild]",
    "13.03   press switch north -> PokemonMansion_B1F (24, 30)",
    "13.03   walk west 9 -> PokemonMansion_B1F (15, 30) [wild]",
    "13.03   walk north 13 (to wall) -> PokemonMansion_B1F (15, 17) [wild]",
    "13.03   walk east 14 (to wall) -> PokemonMansion_B1F (29, 17) [wild]",
    "13.03   walk south 5 -> PokemonMansion_B1F (29, 22) [wild]",
    "13.03   walk east 4 -> PokemonMansion_B1F (33, 22) [wild]",
    "13.03   walk north 17 -> PokemonMansion_B1F (33, 5) [wild]",
    "13.03   walk east 1 -> PokemonMansion_B1F (34, 5) [wild]",
    "13.03 to_tm14 from PokemonMansion_B1F (34, 5) facing east to PokemonMansion_B1F (23, 5) facing north",
    "13.03   walk west 4 -> PokemonMansion_B1F (30, 5) [wild]",
    "13.03   walk south 8 (to wall) -> PokemonMansion_B1F (30, 13) [wild]",
    "13.03   walk west 11 -> PokemonMansion_B1F (19, 13) [wild]",
    "13.03   walk north 10 (to wall) -> PokemonMansion_B1F (19, 3) [wild]",
    "13.03   walk east 5 (to wall) -> PokemonMansion_B1F (24, 3) [wild]",
    "13.03   walk south 2 (to wall) -> PokemonMansion_B1F (24, 5) [wild]",
    "13.03   walk west 1 -> PokemonMansion_B1F (23, 5) [wild]",
    "13.03   face north -> PokemonMansion_B1F (23, 5)",
    "13.04 to_key from PokemonMansion_B1F (23, 5) facing north to PokemonMansion_B1F (5, 6) facing south",
    "13.04   walk west 4 -> PokemonMansion_B1F (19, 5) [wild]",
    "13.04   walk south 8 (to wall) -> PokemonMansion_B1F (19, 13) [wild]",
    "13.04   walk east 11 -> PokemonMansion_B1F (30, 13) [wild]",
    "13.04   walk north 10 (to wall) -> PokemonMansion_B1F (30, 3) [wild]",
    "13.04   walk west 4 (to wall) -> PokemonMansion_B1F (26, 3) [wild]",
    "13.04   walk south 3 (to wall) -> PokemonMansion_B1F (26, 6) [wild]",
    "13.04   walk east 1 -> PokemonMansion_B1F (27, 6) [tapped] [wild]",
    "13.04   press switch north -> PokemonMansion_B1F (27, 6)",
    "13.04   walk east 3 -> PokemonMansion_B1F (30, 6) [wild]",
    "13.04   walk south 4 -> PokemonMansion_B1F (30, 10) [wild]",
    "13.04   walk west 29 (to wall) -> PokemonMansion_B1F (1, 10) [wild]",
    "13.04   walk north 4 -> PokemonMansion_B1F (1, 6) [wild]",
    "13.04   walk east 4 -> PokemonMansion_B1F (5, 6) [wild]",
    "13.04   face south -> PokemonMansion_B1F (5, 6)",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
