/*  Story Check Level
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How much a story step reads the screen to confirm what it did. The
 *  program's options choose the level (StoryRunOptions); the one table here
 *  (runs_check()) says which kind of check runs at which level, and
 *  every check site asks it by its kind, so no step decides on its own.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckLevel_H
#define PokemonAutomation_Pokemon_StoryCheckLevel_H

#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  How much a step reads the screen to confirm what it did, from the most
//  to the least.
enum class StoryCheckLevel{
    //  Everything THOROUGH checks, each checkpoint run 3 times by default to
    //  make sure it really passes.
    TESTING,
    //  Check everything for the user: Bag items, Pokemon stats,
    //  preconditions.
    THOROUGH,
    //  Assume most things are fine; check the most important ones.
    DEFAULT,
    //  Minimal checks.
    FAST,
    //  Almost no checks, to be as fast as possible.
    SPEEDRUN,
};

// Returns the readable string representation of a story check level.
std::string story_check_level_name(StoryCheckLevel level);


//  What a check reads, which decides the lowest level it runs at
//  (runs_check()). A check only verifies: its answer is logged or passes
//  or fails its step, and no later step reads it. A step that acts on what
//  the screen shows (a battle's items, a member's level for a GRIND) reads
//  it itself, at every level, and has no kind here: a precheck skipped at
//  DEFAULT left 19.01's rival fought with 0 Potions and no X item (console
//  B, 2026-11-15).
enum class StoryCheckKind{
    //  After a reload, the save card's place against where the checkpoint
    //  starts: a save that didn't happen leaves an older one.
    SAVE_PLACE,
    //  The screen where a checkpoint starts against the frame kept of it.
    START_SCREEN,
    //  A menu or Bag row's name read before pressing on it: a Mart row
    //  before buying it, an item before using it.
    MENU_ROW,
    //  A tap into the wall a leg walked to, to confirm it ended there.
    LEG_END_TAP,
    //  The Bag's pockets read against the counts the story keeps: after a
    //  reload, after shopping. It changes no count.
    BAG_CONTENTS,
    //  A party member's summary read to check its level or stats before a
    //  fight: a plan's LEVEL step, a precondition.
    PARTY_STATS,
};

//  Whether a run at `level` does checks of `kind`: the one table of the
//  lowest level (the one with the fewest checks) at which each kind still
//  runs; every level with more checks runs it too.
bool runs_check(StoryCheckLevel level, StoryCheckKind kind);
//  "lead_lv50: skipped (checks: default)": the line logged for a check of
//  `name` that `level` skips.
std::string skipped_check_line(std::string_view name, StoryCheckLevel level);
//  The table as the option's help shows it: each kind and the levels that
//  run it, one line each, HTML.
std::string story_checks_help();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
