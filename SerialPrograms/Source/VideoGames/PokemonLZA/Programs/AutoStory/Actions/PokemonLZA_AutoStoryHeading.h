/*  PokemonLZA AutoStory - Heading
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Turning the player. Directions are the stick relative to the camera:
 *  north is straight on, east to the right. story_turn() turns that way;
 *  the backend's stick() walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryHeading_H
#define PokemonAutomation_PokemonLZA_AutoStoryHeading_H

#include <functional>
#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  `degrees` clockwise (negative: left): L recenters the camera, then a
//  150 ms stick tap, which also takes a step.
void story_turn(PlzaBackend& backend, double degrees);

//  The minimap arrow's heading: 0 up, 90 right.
using MinimapArrowReader = std::function<double()>;

//  4 turns at most, then L puts the camera behind the player. The arrow is
//  read only after the queued turns are done, and re-read until it moved
//  half the turn: a lagging feed shows the old facing. Fails as "heading_missed".
std::optional<Pokemon::StoryFailure> face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
