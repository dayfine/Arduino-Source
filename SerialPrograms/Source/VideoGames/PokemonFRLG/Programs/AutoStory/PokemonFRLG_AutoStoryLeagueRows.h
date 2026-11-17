/*  AutoStory League Rows
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's fights as verify rows (Pokemon_StoryVerify.h): each fight
 *  is its own checkpoint, run as the shared engine's verification loop
 *  from its start save. FRLG's part is the pass predicate: the fight the
 *  row's battle names, won (the battle step fails otherwise) spending no
 *  item over the plan's caps (league_try_passes()), leaving our side, as
 *  read off the console, able to start the next fight: its entry contract
 *  met after the heal rule's top-up (league_entry_check()).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueRows_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueRows_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryCheckpoints.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The League fight of `row`'s battle; none if it has none.
const LeagueFightData* league_row_fight(const StoryCheckpoint& row);

//  The fight after `fight` in the League's order; none after the Champion.
const LeagueFightData* next_league_fight(const LeagueFightData& fight);

//  Our side as read off the console: the lead's HP, its moves' PP and the
//  bag, by slug.
struct LeagueSide{
    int hp = 0;
    int max_hp = 0;
    SlugCounts pp = {};
    SlugCounts full_pp = {};
    SlugCounts bag = {};
};

//  `side` against `next`'s entry contract once the heal rule's top-up
//  before it (planned_between()) is done: every item of contract.entry
//  at least its count, the HP at least entry_hp_share of its maximum, each
//  move's PP at least contract.entry's. `compared` logs each value,
//  wanted against had; `short_of` names what falls short (a shopping list
//  for the items).
struct LeagueEntryCheck{
    std::vector<std::string> topped_up = {};
    std::vector<std::string> compared = {};
    std::vector<std::string> short_of = {};
};

//  `side` checked against `next`'s entry contract (LeagueEntryCheck above).
LeagueEntryCheck league_entry_check(const LeagueFightData& next, LeagueSide side);

//  Whether a won try of `row`, its side read as `end`, passes and may be
//  saved: for a row whose battle is a League fight, the items spent from
//  `start_bag` (the start save's) within the fight's caps, and `end` able
//  to start the next fight (league_entry_check()). Any other row passes.
//  The failure's detail logs every value compared.
Pokemon::Outcome league_row_pass(const StoryCheckpoint& row, const SlugCounts& start_bag, const LeagueSide& end);

//  The supplies' counts by bag slug.
SlugCounts supplies_counts(const Supplies& supplies);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
