/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's backend for the engine and the turns every backend takes the same way.
 *
 *  Directions are the stick relative to the camera: north is straight on,
 *  east to the right. story_turn() turns that way; stick() walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_H

#include <functional>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The engine's backend.
using PlzaBackend = Pokemon::StoryBackend;

//  `degrees` clockwise (negative: left): L recenters the camera, then a
//  150 ms stick tap, which also takes a step.
void story_turn(PlzaBackend& backend, double degrees);

//  The minimap arrow's heading: 0 up, 90 right.
using MinimapArrowReader = std::function<double()>;

//  Holds ZL up to `hold`; true once the move-effectiveness symbols show (locked on).
using LockOnWatch = std::function<bool(Milliseconds hold)>;

//  4 turns at most, then L puts the camera behind the player. The arrow is
//  read only after the queued turns are done, and re-read until it moved
//  half the turn: a lagging feed shows the old facing. Fails as "heading_missed".
std::optional<Pokemon::StoryFailure> face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
