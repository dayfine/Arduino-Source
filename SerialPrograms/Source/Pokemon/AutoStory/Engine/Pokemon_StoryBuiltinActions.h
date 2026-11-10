/*  Story Builtin Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The code of the shared built-in actions, written once against the
 *  backend, so every game and both backends do them the same way:
 *
 *      legs    LEGS    a tap per tile; the pose moves along when it was one tile
 *      face    STEP    a tap in `direction`
 *      answer  ANSWER  A for yes, B for no
 *      talk    TALK    A, then a text box must open, then text to the overworld
 *      mash    MASH    text until the player can move
 *      fight   BATTLE  the backend's battle
 *      save    SAVE    the game's save (StoryGame::save)
 *
 *  route, ready, train and shop need a game's map, planner or menus: their
 *  built-in action fails as "unsupported_step" naming the hook, and a game
 *  that has them registers an action of the same kind and name in
 *  StoryGame::actions, which replaces the built-in one.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryBuiltinActions_H
#define PokemonAutomation_Pokemon_StoryBuiltinActions_H

#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The built-in actions, in the order a kind's default comes first: the
//  specs of builtin_story_actions() (Pokemon_BuiltinStoryActions.h), then
//  talk, mash and save, which take no args. `game` gives the save.
template <StoryPose PoseType>
std::vector<StoryAction<PoseType>> builtin_story_handlers(const StoryGame<PoseType>& game);

//  Instantiated once, in Pokemon_StoryBuiltinActions.cpp.
extern template std::vector<StoryAction<GridPose>> builtin_story_handlers<GridPose>(const StoryGame<GridPose>& game);
extern template std::vector<StoryAction<ZonePose>> builtin_story_handlers<ZonePose>(const StoryGame<ZonePose>& game);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
