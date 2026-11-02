/*  SID Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SID Helper's steps from the naming screen to the overworld, which set the
 *  SID by timing the last press of the intro. Shared with AutoStory.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_SidSteps_H
#define PokemonAutomation_PokemonFRLG_SidSteps_H

#include <cstdint>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  When to press the last A of the intro to land on an advance.
struct SidTiming{
    //  From confirming the player's name to the last press.
    uint64_t delay_ms = 0;
    //  The language's intro has one more text box before the last press.
    bool extra_press_at_end = false;
};

//  The timing that sets the SID at "target_advances" (odd) in a game in
//  "language".
SidTiming sid_timing(Language language, uint32_t target_advances);

//  From the player's naming screen, confirms the name, picks the default
//  rival name and presses through the intro at "timing", which sets the SID.
//  Runs blind; ends on the last press of the intro.
void set_sid_from_name_screen(ProControllerContext& context, const SidTiming& timing);

//  After set_sid_from_name_screen(): waits out the black screen into the
//  overworld. Throws OperationFailedException if no black screen comes
//  within 10 seconds.
void finish_intro_animations(SingleSwitchProgramEnvironment& env, ProControllerContext& context);



}
}
}
#endif
