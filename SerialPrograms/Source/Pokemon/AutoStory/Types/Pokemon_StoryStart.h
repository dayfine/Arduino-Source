/*  AutoStory Story Start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a story run reaches its start checkpoint, and what that allows.
 *  The start sequence itself is prepare_story_start() in Pokemon_StoryRun.h.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStart_H
#define PokemonAutomation_Pokemon_StoryStart_H

namespace PokemonAutomation{
namespace Pokemon{


//  How the game reaches the start checkpoint.
enum class StoryStart{
    //  A new game from the title screen: there is no save.
    NEW_GAME,
    //  The game as another program left it: nothing is loaded.
    LIVE,
    //  The checkpoint's save, loaded first.
    FROM_SAVE,
};

//  Whether the first checkpoint's own save at its start may run: only a
//  new game's. After a load the save is already there, and from the game
//  as it is the place is unconfirmed.
inline bool story_first_save_allowed(StoryStart start){
    return start == StoryStart::NEW_GAME;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
