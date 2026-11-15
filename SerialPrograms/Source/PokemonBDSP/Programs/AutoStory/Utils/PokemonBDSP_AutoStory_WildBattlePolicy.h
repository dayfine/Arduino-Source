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
    NO_RUNNING, //  "There's no running from a Trainer battle!": not a wild battle
};

//  The game steps escape_wild_battle() takes, injected so its decisions run
//  in tests without a Switch: the console presses the buttons and watches
//  the screen; the simulation scripts the outcomes.
class WildBattleEscapeActions{
public:
    virtual ~WildBattleEscapeActions() = default;

    //  On the battle menu: Run, then watch for the exit or the menu again.
    virtual WildRunOutcome run() = 0;

    //  On the battle menu after a refused Run: fight with the best move until
    //  the battle ends, on the overworld when it returns.
    virtual void fight() = 0;

    //  After NO_RUNNING, on the battle menu: the trainer battle as the story's
    //  trainer battles are fought (best move to the end, then a potion if the
    //  lead is low), on the overworld when it returns.
    virtual void fight_trainer() = 0;

    //  A line of the escape's log.
    virtual void note(const std::string& text) = 0;
};

//  What to do once the game has refused the escape `run_refusals` times.
enum class TrappedFallback{
    GIVE_UP,    //  keep pressing Run until the attempts run out
    FIGHT,      //  fight the battle to its end instead
};

//  How a wild battle is left: data, so the simulation runs the console's
//  policy, and a later shared policy can carry it.
struct WildBattleEscapePolicy{
    //  run() calls in all before the escape is given up as unconfirmed.
    size_t attempts = 12;
    //  Refused Runs (the menu back each time) before `fallback` applies: a
    //  trapping move such as Bind refuses every one of them.
    size_t run_refusals = 3;
    TrappedFallback fallback = TrappedFallback::FIGHT;
};

//  How escape_wild_battle() ended.
enum class WildEscapeResult{
    ESCAPED,
    FOUGHT,         //  the game kept refusing the escape; the battle was fought to its end
    TRAINER_FOUGHT, //  a Trainer battle, which cannot be run from: fought as the story's are
    UNCONFIRMED,    //  the attempts ran out without the exit
};

//  The decisions of escape_wild_battle(): Run on the battle menu until the
//  exit shows, within `policy.attempts` tries; after `policy.run_refusals`
//  refused Runs, `policy.fallback`. A Run the game answers with "no running
//  from a Trainer battle" ends the attempts at once: it is fought as a
//  trainer battle (a wild battle's refusals are worth repeating, a trainer's
//  never change: lab-B 2026-11-12 14:55, Route 203, five Runs and a faint
//  misread before it was fought).
WildEscapeResult escape_wild_battle(WildBattleEscapeActions& actions, const WildBattleEscapePolicy& policy);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
