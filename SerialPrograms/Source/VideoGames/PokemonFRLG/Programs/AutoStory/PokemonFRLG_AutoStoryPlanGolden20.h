/*  FRLG AutoStory Plan Golden, Segment 20
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden's lines for 20.01a and after (Indigo Plateau's Mart, the
 *  Elite Four and the Champion), in the story's order, after
 *  PokemonFRLG_AutoStoryPlanGolden19.h's 19.01a to 19.05.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden20_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlanGolden20_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The stored plan lines of 20.01a and after.
inline constexpr std::string_view FRLG_PLAN_GOLDEN_20[] = {
    "20.01a indigo_pc_front from IndigoPlateau_PokemonCenter_1F (13, 12) facing north to IndigoPlateau_PokemonCenter_1F (17, 10) facing north",
    "20.01a   walk east 4 -> IndigoPlateau_PokemonCenter_1F (17, 12) [walk only]",
    "20.01a   walk north 2 (to wall) -> IndigoPlateau_PokemonCenter_1F (17, 10) [walk only]",
    "20.01a indigo_mart_clerk from IndigoPlateau_PokemonCenter_1F (17, 10) facing north to IndigoPlateau_PokemonCenter_1F (2, 7) facing west",
    "20.01a   walk south 2 -> IndigoPlateau_PokemonCenter_1F (17, 12) [walk only]",
    "20.01a   walk west 13 -> IndigoPlateau_PokemonCenter_1F (4, 12) [walk only]",
    "20.01a   walk north 5 -> IndigoPlateau_PokemonCenter_1F (4, 7) [walk only]",
    "20.01a   walk west 2 (to wall) -> IndigoPlateau_PokemonCenter_1F (2, 7) [walk only]",
    "20.01 below_league_door from IndigoPlateau_PokemonCenter_1F (2, 7) facing west to IndigoPlateau_PokemonCenter_1F (4, 2) facing north",
    "20.01   walk east 1 -> IndigoPlateau_PokemonCenter_1F (3, 7) [walk only]",
    "20.01   walk north 5 (to wall) -> IndigoPlateau_PokemonCenter_1F (3, 2) [walk only]",
    "20.01   walk east 1 -> IndigoPlateau_PokemonCenter_1F (4, 2) [walk only]",
    "20.01   face north -> IndigoPlateau_PokemonCenter_1F (4, 2) [walk only]",
    "20.01 before_lorelei from PokemonLeague_LoreleisRoom (6, 7) facing north to PokemonLeague_LoreleisRoom (6, 6) facing north",
    "20.01   walk north 1 (to wall) -> PokemonLeague_LoreleisRoom (6, 6) [walk only]",
    "20.02 below_loreleis_door from PokemonLeague_LoreleisRoom (6, 6) facing north to PokemonLeague_LoreleisRoom (6, 3) facing north",
    "20.02   walk east 1 -> PokemonLeague_LoreleisRoom (7, 6) [walk only]",
    "20.02   walk north 3 (to wall) -> PokemonLeague_LoreleisRoom (7, 3) [walk only]",
    "20.02   walk west 1 -> PokemonLeague_LoreleisRoom (6, 3) [walk only]",
    "20.02   face north -> PokemonLeague_LoreleisRoom (6, 3) [walk only]",
    "20.02 before_bruno from PokemonLeague_BrunosRoom (6, 7) facing north to PokemonLeague_BrunosRoom (6, 6) facing north",
    "20.02   walk north 1 (to wall) -> PokemonLeague_BrunosRoom (6, 6) [walk only]",
    "20.03 below_brunos_door from PokemonLeague_BrunosRoom (6, 6) facing north to PokemonLeague_BrunosRoom (6, 3) facing north",
    "20.03   walk east 1 -> PokemonLeague_BrunosRoom (7, 6) [walk only]",
    "20.03   walk north 3 (to wall) -> PokemonLeague_BrunosRoom (7, 3) [walk only]",
    "20.03   walk west 1 -> PokemonLeague_BrunosRoom (6, 3) [walk only]",
    "20.03   face north -> PokemonLeague_BrunosRoom (6, 3) [walk only]",
    "20.03 before_agatha from PokemonLeague_AgathasRoom (6, 7) facing north to PokemonLeague_AgathasRoom (6, 6) facing north",
    "20.03   walk north 1 (to wall) -> PokemonLeague_AgathasRoom (6, 6) [walk only]",
    "20.04 below_agathas_door from PokemonLeague_AgathasRoom (6, 6) facing north to PokemonLeague_AgathasRoom (6, 3) facing north",
    "20.04   walk east 1 -> PokemonLeague_AgathasRoom (7, 6) [walk only]",
    "20.04   walk north 3 (to wall) -> PokemonLeague_AgathasRoom (7, 3) [walk only]",
    "20.04   walk west 1 -> PokemonLeague_AgathasRoom (6, 3) [walk only]",
    "20.04   face north -> PokemonLeague_AgathasRoom (6, 3) [walk only]",
    "20.04 before_lance from PokemonLeague_LancesRoom (6, 10) facing north to PokemonLeague_LancesRoom (6, 9) facing north",
    "20.04   walk north 1 (to wall) -> PokemonLeague_LancesRoom (6, 9) [walk only]",
    "20.05 below_champions_door from PokemonLeague_LancesRoom (6, 9) facing north to PokemonLeague_LancesRoom (6, 6) facing north",
    "20.05   walk east 1 -> PokemonLeague_LancesRoom (7, 9) [walk only]",
    "20.05   walk north 3 (to wall) -> PokemonLeague_LancesRoom (7, 6) [walk only]",
    "20.05   walk west 1 -> PokemonLeague_LancesRoom (6, 6) [walk only]",
    "20.05   face north -> PokemonLeague_LancesRoom (6, 6) [walk only]",
};

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
