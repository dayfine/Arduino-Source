/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Types/Pokemon_StoryStep.h), plain data as BDSP's and
 *  FRLG's tables are, so unit tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a walk in the overworld is a GAME "walk" step whose args hold its
 *  legs: a relative turn and a walk time each (PokemonLZA_AutoStoryGame.h).
 *
 *  Recorded 2026-11-08/09 on console B, played step by step from a new
 *  game to the quest "Return to Hotel Z to meet up with Taunie" in Main
 *  Mission 03. Every walk was measured walking, not dashing. Steps the game
 *  has no code for yet are GAME steps of story_actions_not_implemented().
 *
 *  This is the story as data, with no IO: the checkpoint files it is read
 *  from, built in or from the dev folder, are PokemonLZA_AutoStoryStoryFiles.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story a run plays: its checkpoints in order, and the ids of those
//  that end on one of the game's autosaves and do not save themselves
//  (their JSON "execution": {"save": "autosave"}).
struct StoryData{
    std::vector<Pokemon::StoryCheckpoint> checkpoints;
    std::set<std::string, std::less<>> autosaves;
    //  The ids of the rows a run reloads the save before when it played the
    //  row before them too ("execution": {"reload_when_chained": true}), so
    //  they start as from their save. Each comes after an autosave row,
    //  whose autosave is its start save.
    std::set<std::string, std::less<>> reload_when_chained;
    //  The dev files that replaced a built-in checkpoint, as (file name,
    //  path).
    std::vector<std::pair<std::string, std::string>> dev_files;
};

//  How a run plays its checkpoints: the attempts each gets, and whether it
//  reloads at the plan's boundaries.
struct StoryRunPolicy{
    //  Whether a run of several checkpoints reloads the save at a row of
    //  StoryData::reload_when_chained (AT_PLAN_BOUNDARIES) or runs on from
    //  the state the row before left (STRAIGHT_THROUGH). A failed
    //  checkpoint's retries reload as usual either way.
    enum class BoundaryReload{
        AT_PLAN_BOUNDARIES,
        STRAIGHT_THROUGH,
    };
    size_t attempts_per_checkpoint = 1;
    BoundaryReload boundary_reload = BoundaryReload::AT_PLAN_BOUNDARIES;
};

//  Whether `row` of `story` saves through the menu at its end: every row
//  but the autosave rows. The menu (X, then R) can save from the start of
//  the story too: 01.03 saves through it after Andi (console B, 2026-11-10).
bool story_row_saves(const StoryData& story, const Pokemon::StoryCheckpoint& row);

//  How many rows of `checkpoints` from the first need no action of
//  story_actions_not_implemented(): the part of the story a run can play.
size_t story_rows_ready(const std::vector<Pokemon::StoryCheckpoint>& checkpoints);

//  Whether `row` starts by launching the game from HOME (its first step is
//  the GAME action "launch_game_from_home"): 00.00. The game may be in front on any
//  screen, a dark scene too, so the frame at the program's start says nothing
//  about the screen size and the black border check waits for HOME.
bool story_row_launches_game(const Pokemon::StoryCheckpoint& row);

//  Whether `row` starts on the new game's "Confirm Choices" screen, the
//  language, look and name chosen by hand (its first step is the GAME action
//  "confirm_new_game"): 01.00. It plays what 00.00 plays after the name.
bool story_row_confirms_new_game(const Pokemon::StoryCheckpoint& row);

//  Whether `row` starts with no save to load: it launches the game or
//  confirms a new game.
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);

//  The rows a run from `first` to `last` of `checkpoints` plays, in order:
//  all of them but a row that confirms a new game after the run's first
//  row, which played it already. Empty if `first` comes after `last` or
//  `last` is past the end.
std::vector<Pokemon::StoryCheckpoint> story_run_rows(
    const std::vector<Pokemon::StoryCheckpoint>& checkpoints, size_t first, size_t last
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
