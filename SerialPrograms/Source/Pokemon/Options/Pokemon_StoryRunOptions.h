/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for how hard to try: the
 *  same group in each game, read into the shared SegmentPolicy
 *  (Pokemon/Navigation/Pokemon_StorySegments.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


class StoryRunOptions : public GroupOption{
public:
    StoryRunOptions();

    //  The policy these options set. In-place recoveries are the game's to
    //  set, not the user's.
    SegmentPolicy policy() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption STOP_AT_FIRST_FAILURE;
};



}
}
#endif
