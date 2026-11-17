/*  Story Verify
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint run as a verification loop (a plan's "execution":
 *  {"verify": {"tries": 10, "min_pass": 5, "reset_budget": 5}}): phase 1
 *  plays it `tries` times from its start save, saving none, and stops the
 *  run when fewer than `min_pass` passed; phase 2 plays it again, up to
 *  `reset_budget` times, until one passes, and saves that one. A try
 *  passes when it completes, the game's postcondition check holds and the
 *  game's pass predicate holds (SegmentHooks, Pokemon_StorySegments.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryVerify_H
#define PokemonAutomation_Pokemon_StoryVerify_H

#include <cstddef>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  A verify pass check that couldn't read the game
//  (SegmentHooks::verify_pass): the loop stops, with no save.
inline constexpr std::string_view VERIFY_UNREAD = "postcondition_unread";

//  The verification loop's figures.
struct StoryVerify{
    size_t tries = 10;
    size_t min_pass = 5;
    size_t reset_budget = 5;

    bool operator==(const StoryVerify&) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
