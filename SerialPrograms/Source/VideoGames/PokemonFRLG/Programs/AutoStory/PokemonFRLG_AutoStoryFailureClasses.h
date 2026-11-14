/*  AutoStory Failure Classes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's failure names on the shared story failure classes
 *  (Pokemon/AutoStory/Pokemon_StoryFailureClasses.h): the names FRLG's
 *  steps fail with, and the "class=<id>" tag its run log puts on each
 *  failure line, so a run record reads a failure's class back the same way
 *  in every game.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFailureClasses_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFailureClasses_H

#include <optional>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_StoryFailureClasses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The class of the failure name `failure` ("no_pose", "mart_no_money"):
//  FRLG's own names, then the families its steps name by prefix
//  ("mart_", "fly_"), then the engine's names
//  (Pokemon::story_failure_class_of_outcome()). Nothing for a name no
//  class lists.
std::optional<Pokemon::StoryFailureClass> frlg_failure_class(std::string_view failure);

//  `line` with " class=<id>" (Pokemon::story_failure_class_tag()) at its
//  end when it is a failure line whose failure name has a class:
//      [Event] 08.01/3 winston_line: failed: no_trainer: 3 s on the tile
//      [Checkpoint] 08.01: failed at 3 winston_line: no_trainer: 3 s on the tile
//      [Story] reload_failed: Failed to reset
//  Any other line, or one already tagged, comes back unchanged.
std::string tag_failure_line(std::string_view line);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
