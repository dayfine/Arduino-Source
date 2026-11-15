/*  Story Check Level
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How much a story step reads the screen to confirm what it did. The
 *  program's options choose it (StoryRunOptions); each game decides what
 *  its steps check at each level.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckLevel_H
#define PokemonAutomation_Pokemon_StoryCheckLevel_H

#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  How much a step reads the screen to confirm what it did.
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



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
