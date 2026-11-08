/*  BDSP AutoStory - Wild battle escape policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a wild battle that interrupts a walk is left, as decisions over the
 *  game steps the console or the simulation supplies, so the same policy
 *  runs on the Switch and in GridSim tests.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WildBattlePolicy_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WildBattlePolicy_H

#include <cstddef>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What one Run on the battle menu came to.
enum class WildRunOutcome{
    ESCAPED,    //  the battle's exit transition showed
    TRAPPED,    //  the battle menu came back: the game refused the escape
    UNCERTAIN,  //  neither showed within the wait: text still scrolling, cursor unread
};

//  The game steps escape_wild_battle() takes, injected so its decisions run
//  in tests without a Switch: the console presses the buttons and watches
//  the screen; the simulation scripts the outcomes.
class WildBattleEscapeActions{
public:
    virtual ~WildBattleEscapeActions() = default;

    //  On the battle menu: Run, then watch for the exit or the menu again.
    virtual WildRunOutcome run() = 0;

    //  A line of the escape's log.
    virtual void note(const std::string& text) = 0;
};

//  How a wild battle is left: data, so the simulation runs the console's
//  policy, and a later shared policy can carry it.
struct WildBattleEscapePolicy{
    //  run() calls in all before the escape is given up as unconfirmed.
    size_t attempts = 12;
};

//  How escape_wild_battle() ended.
enum class WildEscapeResult{
    ESCAPED,
    UNCONFIRMED,    //  the attempts ran out without the exit
};

//  The decisions of escape_wild_battle(): Run on the battle menu until the
//  exit shows, within `policy.attempts` tries.
WildEscapeResult escape_wild_battle(WildBattleEscapeActions& actions, const WildBattleEscapePolicy& policy);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
