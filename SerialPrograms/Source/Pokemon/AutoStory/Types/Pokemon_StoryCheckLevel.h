/*  Story Checks
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

namespace PokemonAutomation{
namespace Pokemon{


//  How much a step reads the screen to confirm what it did.
enum class StoryChecks{
    //  Confirm on screen: purchases, the Bag, status after battles, position.
    THOROUGH,
    //  In a rush: trust counts and timings.
    FAST,
};

// Returns the readable string representation of a story check level.
const char* story_checks_name(StoryChecks checks);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
