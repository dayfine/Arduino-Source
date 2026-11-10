/*  Story Step Kind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The kinds of story step the tile games (FRLG, BDSP) share. A game's
 *  executor does each kind its own way (Pokemon_StoryStepExecutor.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepKind_H
#define PokemonAutomation_Pokemon_StoryStepKind_H

namespace PokemonAutomation{
namespace Pokemon{


// Action category classification for individual story steps.
enum class StoryStepKind{
    NEW_GAME,   //  title screen (no save) to the first room
    OPTIONS,    //  the game's options as the story needs them
    ROUTE,      //  plan and walk to `target`
    LEGS,       //  walk `legs` as given
    STEP,       //  one step `direction`: onto a trigger, into a door
    TALK,       //  A at what the player faces
    ANSWER,     //  text to a YES/NO, then `yes`
    MASH,       //  text until the overworld is back: a scene
    BATTLE,     //  text into a battle, fight it, text to the overworld
    TRAIN,      //  fight `battles` wild battles where the player stands
    HEAL,       //  the nurse, from in front of her or of the Center's door
    SHOP,       //  `items` from a shop
    SAVE,       //  save the game where the player stands
    READY,      //  before a big battle: items, and the nurse at `target` if they can't
    GAME,       //  a step only this game has, named by `variant`
};

// Returns the readable string representation of a story step kind.
const char* story_step_kind_name(StoryStepKind kind);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
