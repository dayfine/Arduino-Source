/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for how hard to try and how
 *  much to check: the same group in each game, read into the shared
 *  SegmentPolicy (Pokemon/Navigation/Pokemon_StorySegments.h) and the
 *  executor's checks level (Pokemon_StorySteps.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace Pokemon{


class StoryRunOptions : public GroupOption{
public:
    StoryRunOptions();

    //  The policy these options set. In-place recoveries are the game's to
    //  set, not the user's.
    SegmentPolicy policy() const;
    //  The checks level for the game's story step executor (set_checks()).
    StoryChecks checks() const;
    //  The game's language, for the executor (set_language()).
    Language language() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption STOP_AT_FIRST_FAILURE;
    EnumDropdownOption<StoryChecks> CHECKS;
    //  Any language the game has, whether or not a reader supports it:
    //  steps without one use language-free checks, so none is refused.
    EnumDropdownOption<Language> LANGUAGE;
};



}
}
#endif
