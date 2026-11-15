/*  Center Visit
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A Pokemon Center visit for every game: in through the door, wait out
 *  the fade-in, heal, out through the door, wait out the fade, and the two
 *  checks that the doors took us where they should. A game gives the
 *  walks, the reads and the nurse through CenterVisitIO; the order, the
 *  settles and the checks are here.
 *
 */

#ifndef PokemonAutomation_Pokemon_CenterVisit_H
#define PokemonAutomation_Pokemon_CenterVisit_H

#include <string_view>
#include "Pokemon_SceneJudges.h"
#include "Pokemon_Settle.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where a game's reads say the player is.
enum class CenterPlace{
    INSIDE,     //  a Center's floor or counter shows
    OUTSIDE,    //  something else shows
    UNKNOWN,    //  the game cannot tell from this screen
};

//  The failure of a visit that is not in the Center after walking in.
inline constexpr std::string_view CENTER_NOT_ENTERED = "heal_enter";
//  The failure of a visit still in the Center after walking out.
inline constexpr std::string_view CENTER_NOT_LEFT = "heal_exit";


//  What a visit needs of a game. Its walks and the nurse fail by throwing,
//  as the game's own steps do.
class CenterVisitIO{
public:
    virtual ~CenterVisitIO() = default;

    //  From below the door, walks in. Returns what it saw of the warp.
    virtual WarpSeen walk_in() = 0;
    //  Lines up with the door by sight and walks in: the walk in saw no
    //  warp and the inside does not show. Returns what it saw of the warp.
    virtual WarpSeen line_up_and_walk_in() = 0;
    virtual CenterPlace place() = 0;
    //  From inside the door, walks to the nurse, heals, and clears her
    //  last line.
    virtual void heal() = 0;
    //  From the nurse, walks out. Returns what it saw of the warp.
    virtual WarpSeen walk_out() = 0;
    //  Waits until the game settles (run_settle() or the sim's own). A
    //  visit fails on any end but QUIET, with the end's text.
    virtual SettleEnd settle(SettleKind kind, std::string_view label) = 0;
    virtual void log(std::string_view line) = 0;
};

//  Walks in and waits out the fade-in. Fails CENTER_NOT_ENTERED when the
//  settle does not end quiet or the reads then say outside: the nurse's
//  presses would land elsewhere (FRLG console B 2026-11-13, 12.03).
//  UNKNOWN passes.
Outcome enter_center(std::string_view label, CenterVisitIO& io);
//  Walks out and waits out the fade. Fails CENTER_NOT_LEFT when the
//  settle does not end quiet or the reads then say inside (BDSP r143a).
//  UNKNOWN passes.
Outcome leave_center(std::string_view label, CenterVisitIO& io);
//  enter_center(), the nurse, leave_center(): from below the door back to
//  below it.
Outcome run_center_visit(std::string_view label, CenterVisitIO& io);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
