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
    //  Confirm on screen: purchases, the Bag, status after battles, position.
    THOROUGH,
    //  In a rush: trust counts and timings.
    FAST,
};

// Returns the readable string representation of a story check level.
std::string story_check_level_name(StoryCheckLevel level);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
