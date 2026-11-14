/*  AutoStory Teach Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game steps a TEACH step takes, injected so its decisions run in
 *  tests, and how each of them ends.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTeachActions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTeachActions_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What A on the member showed.
enum class TeachPrompt{
    LEARNED,
    //  "Delete an older move to make room?"
    DELETE_PROMPT,
    NOT_COMPATIBLE,
    ALREADY_KNOWS,
    FAILED,
};

//  The outcome of attempting to forget a move.
enum class ForgetEnd{
    LEARNED,
    //  "HM moves can't be forgotten now.": back on the moves page.
    HM_REFUSED,
    FAILED,
};

//  What opening the TM or HM from the bag came to.
enum class ItemOpen{
    //  On "Teach which POKEMON?".
    OPENED,
    //  The TM CASE's list was read and hasn't got it.
    NOT_IN_CASE,
    FAILED,
};

//  The game steps a TEACH step takes, injected so its decisions run in
//  tests.
class TeachActions{
public:
    virtual ~TeachActions() = default;

    //  From the overworld: the bag, the TM Case, `item` (which holds
    //  `move`), USE, to the party menu.
    virtual ItemOpen open_item(std::string_view item, std::string_view move) = 0;
    //  On "Teach which POKEMON?": A on `slot`.
    virtual TeachPrompt choose_member(size_t slot) = 0;
    //  On "Delete an older move?": YES, then A on move `slot` (0-3).
    virtual ForgetEnd forget(size_t slot) = 0;
    //  From any of its prompts: give up teaching (NO, "Stop trying?" YES).
    virtual void stop_teaching() = 0;
    //  Back to the overworld. False if it didn't come back.
    virtual bool leave() = 0;
    //  Saves the screen as a debug image, named `label`: the step failed.
    virtual void dump_screen(std::string_view label) = 0;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
