/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps, plain data with no IO.
 *  Lumiose has no tile grid: poses name a place only, and a walk is a GAME
 *  "walk" step whose args hold timed legs (PokemonLZA_AutoStoryGame.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The rows the plans' "execution" blocks mark, by id.
struct StoryRows{
    //  "save": "autosave": a run's log showed the game autosave at the row's end.
    std::set<std::string, std::less<>> autosaves = {};
    //  "save": "unverified": believed to autosave there, not yet seen in a run's log.
    std::set<std::string, std::less<>> unverified_autosaves = {};
    //  "reload_when_chained": the row reloads its start save after the row before it.
    std::set<std::string, std::less<>> reload_when_chained = {};
};

//  The story's checkpoints in order, and the rows their plans mark.
struct StoryData : StoryRows{
    std::vector<Pokemon::StoryCheckpoint> checkpoints = {};
};

//  The action of the row's first step; empty for a row with no steps.
std::string_view story_row_first_action(const Pokemon::StoryCheckpoint& row);

//  A new game: the first step is "new_game_intro" (from HOME, 01.00a) or
//  "confirm_new_game" (on "Confirm Choices", 01.00b).
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);

//  The first step is "new_game_intro", which types the name (01.00a).
bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row);

//  "<step>: <rule>" for each step of the plan `checkpoint` (its JSON) not
//  right after the step it needs: a "talk" after a walk ending on an A
//  prompt; a "story_battle" after "story_battle_start"; a scene ("a") after
//  what starts it (a walk, talk, choice or battle), not after a step that
//  presses through to control, which would play the scene itself. A bare
//  "mash" may follow anything.
std::vector<std::string> story_row_lead_in_errors(const JsonObject& checkpoint);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
