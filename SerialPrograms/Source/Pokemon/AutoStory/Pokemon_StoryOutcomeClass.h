/*  Story Outcome Class
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The class (Pokemon_StoryFailureClasses.h) of a failed Outcome, and the
 *  text a failure's log line ends with.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryOutcomeClass_H
#define PokemonAutomation_Pokemon_StoryOutcomeClass_H

#include <optional>
#include <string>
#include "Pokemon_StoryFailureClasses.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The class of a failed `outcome`: its own, else the shared list's for its
//  name. Nothing for a success or a name no class lists.
std::optional<StoryFailureClass> failure_class_of(const Outcome& outcome);

//  to_string() and the class, as a failure's log line ends:
//  "expect_failed: has_parcel (class=state_check_failed)"; without a class,
//  to_string() alone.
std::string to_string_with_class(const Outcome& outcome);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
