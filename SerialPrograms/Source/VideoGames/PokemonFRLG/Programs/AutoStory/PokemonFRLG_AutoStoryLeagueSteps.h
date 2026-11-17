/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's own step. Each fight is its own row (20.01 to 20.05), run
 *  as a verification loop from its start save (Pokemon_StoryVerify.h).
 *
 *  league_top_up (fight WORD, the plan's name of the row's fight; max_hp
 *  INTEGER, the lead's maximum HP when its summary wasn't read;
 *  full_if_unknown BOOLEAN, default false): the row's first step, after
 *  the reload and before the walk. It reads the Items pocket and the
 *  lead's HP (the story's own count since the save, else the summary's
 *  POKEMON SKILLS), uses the plan's top-up before the fight one item at a
 *  time (planned_between(): a Super Potion for up to 50 missing HP, two
 *  for up to 100 if two are spare, else a Full Restore; the Elixir only
 *  before the Champion), then checks the fight's entry contract. A broken
 *  contract or an HP still unknown fails with "precondition_failed": the
 *  save does not meet the row's start, and the loop stops. Never a fight
 *  on an unknown HP (Bruno, console B, 2026-11-16: entered at 71%).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueSteps_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryLeagueRows.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step at a League fight row's start (league_top_up above).
inline const std::string LEAGUE_TOP_UP_ACTION = "league_top_up";

//  On the console, before a League fight row's save: whether the try may
//  be saved (league_row_pass()), from our side as read off the console
//  now, not the story's own counts: the Items pocket, the lead's HP
//  (summary_current_hp()) and its moves' PP (the summary's KNOWN MOVES).
//  Every value compared is logged,
//  wanted against read. A read that fails or is unsure fails
//  "postcondition_unread" (the loop stops: never a save on a guess), with
//  the screen kept in DebugDumps/AutoStory/league.
Pokemon::Outcome check_league_save(StoryEnv& env, const StoryCheckpoint& row);

//  LEAGUE_TOP_UP_ACTION's spec.
std::vector<Pokemon::StoryActionSpec> league_action_specs();

//  On the console, a LEAGUE_TOP_UP_ACTION step; nothing for another.
std::optional<Pokemon::Outcome> run_league_step(StoryEnv& env, const Pokemon::StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
