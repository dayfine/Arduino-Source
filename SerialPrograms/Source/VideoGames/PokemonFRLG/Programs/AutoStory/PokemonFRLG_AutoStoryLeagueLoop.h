/*  AutoStory League Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League fight by fight (the user's design, 2026-11-17): each fight
 *  starts from its own save, where the fight before it was won. A try
 *  loads that save, tops our side up (planned_between()), checks the
 *  fight's entry contract, fights, and passes on a win that spends no item
 *  over the fight's caps and leaves its reserves.
 *
 *  Phase 1: the plan's min_pass_of tries (10), none saved. Fewer passes
 *  than min_pass: the run stops. Phase 2: up to reset_budget more tries,
 *  until one passes, which is saved. A try that a Roar, a Whirlwind or the
 *  plan's early reset ended is a try that didn't pass.
 *
 *  The loop drives a LeagueFightRunner: the console's, or a fake one that
 *  plays scripted battles (PokemonFRLG_AutoStoryLeagueLoop_Tests.cpp).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueLoop_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueLoop_H

#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryLeagueFightData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Our side as the loop reads it: the lead's HP, its moves' PP and the bag.
struct LeagueSide{
    int hp = 0;
    int max_hp = 0;
    SlugCounts pp = {};
    SlugCounts bag = {};
};

//  How a try's battle ended.
enum class LeagueBattleEnd{
    WON,
    LOST,
    //  A Roar or Whirlwind landed in a fight that resets on one; the game
    //  was soft reset.
    ROARED,
    //  The plan's early reset (Bruno's after Onix #1); the game was soft reset.
    EARLY_RESET,
};

//  Where the loop's lines go.
using LeagueLog = std::function<void(std::string_view line)>;

//  What the loop drives.
struct LeagueFightRunner{
    //  Back to the fight's start save.
    std::function<void()> load_start;
    //  The top-ups before the fight (planned_between()); the items used.
    std::function<std::vector<std::string>()> top_up;
    //  Our side as it is now.
    std::function<LeagueSide()> read_side;
    //  The fight, to its end.
    std::function<LeagueBattleEnd()> fight;
    //  Saves the game where it is.
    std::function<void()> save;
    LeagueLog log;
};

//  How the loop left a fight.
enum class LeagueFightVerdict{
    //  A phase 2 try passed and was saved: on to the next fight.
    SAVED,
    //  Phase 1 passed fewer than the plan's min_pass.
    BELOW_MIN_PASS,
    //  No phase 2 try passed within the reset budget.
    OUT_OF_BUDGET,
    //  A try's side broke the fight's entry contract, before fighting.
    PRECONDITION_FAILED,
    //  The plan has no pass figures for the fight.
    NO_FIGURES,
};

//  One try.
struct LeagueTryRecord{
    int phase = 1;
    std::vector<std::string> top_up = {};
    LeagueBattleEnd end = LeagueBattleEnd::LOST;
    //  Every item the try spent, the top-ups included; empty unless won.
    SlugCounts used = {};
    bool passes = false;
};

//  The tries of a fight and how the loop left it.
struct LeagueFightReport{
    LeagueFightVerdict verdict = LeagueFightVerdict::NO_FIGURES;
    std::vector<LeagueTryRecord> tries = {};
    int phase1_passes = 0;
    //  What broke the entry contract, or the postcondition of the last try.
    std::string detail = {};
};

//  `fight` through phases 1 and 2 on `runner`, every try logged with its
//  counts.
LeagueFightReport run_league_fight(const LeagueFightData& fight, const LeagueFightRunner& runner);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
