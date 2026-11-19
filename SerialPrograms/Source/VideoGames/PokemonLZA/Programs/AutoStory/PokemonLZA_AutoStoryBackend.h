/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The backend is what a story step acts through: observe() the screen,
 *  press() and stick() the controller, wait(), log(). The shared engine
 *  defines it (Pokemon::StoryBackend); the console implements it with the
 *  video feed and the controller, the logic tests with a scripted game,
 *  so every PLZA action runs unchanged in both. PLZA adds nothing to it:
 *  this is only PLZA's name for it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_H

#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


using PlzaBackend = Pokemon::StoryBackend;



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
