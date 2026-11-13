/*  Badge Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  After a gym leader's battle, a thorough run opens the Trainer Card and
 *  checks the badge is on it: a battle can end with a win that was not the
 *  leader's (BDSP 01.04, lab-B 2026-11-12: the second gym trainer was taken
 *  for Roark and 01.05 saved with no badge). Each game reads its own card
 *  and says what it saw; the verdict and the wording are shared.
 *
 */

#ifndef PokemonAutomation_Pokemon_BadgeCheck_H
#define PokemonAutomation_Pokemon_BadgeCheck_H

#include <string>
#include <string_view>
#include "Pokemon_StoryCheckLevel.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a game's Trainer Card reader saw for one badge.
enum class BadgeReading{
    PRESENT,
    MISSING,
    //  The reader has no reference for the card yet, or the card was not on
    //  screen. A badge not seen is not a badge confirmed: this fails too.
    UNREADABLE,
};

//  Whether a run at `level` opens the Trainer Card after a leader's battle:
//  the levels that check everything for the user.
bool badge_check_wanted(StoryCheckLevel level);

//  The failure text when the badge is missing or the card was not read, ""
//  when the badge was seen. The check runs only at the levels that check
//  everything (badge_check_wanted()), and there an unreadable card must not
//  pass as a badge confirmed; the caller keeps the card's screenshot.
std::string badge_check_failure(BadgeReading reading, std::string_view badge);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
