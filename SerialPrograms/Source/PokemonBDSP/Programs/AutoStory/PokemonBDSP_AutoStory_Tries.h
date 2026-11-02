/*  BDSP AutoStory - Bounded tries
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How the story's steps cope with a beat that may not take the first
 *  time (an A that meets nobody, a person a row off): a few tries, each
 *  followed by a look at the end state, with a safe move between two (a B,
 *  a row over), and a failed step after the last. No step depends on the
 *  exact screens in between, only on the end state it checks.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Tries_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Tries_H

#include <cstddef>
#include <functional>
#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Up to `tries` calls of `attempt` (numbered from 1), each returning
//  whether the end state holds; `between` runs after each that did not,
//  except the last. Returns the attempt that took, none if none did.
std::optional<size_t> try_bounded(
    size_t tries,
    const std::function<bool(size_t attempt)>& attempt,
    const std::function<void(size_t attempt)>& between
);



}
}
}
#endif
